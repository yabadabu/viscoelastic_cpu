#pragma once

struct CPUSpatialSubdivision {

	using u32 = uint32_t;

	struct Int3 {
		int x, y, z;
		Int3() = default;
		Int3(int ix, int iy, int iz) : x(ix), y(iy), z(iz) {}
		inline bool operator==(const Int3& other) const {
			return (x == other.x && y == other.y && z == other.z);
		}
	};

	// Max cells in hash grid
	static constexpr int         num_cells = 1024 * 64;
	static constexpr int         hash_mask = num_cells - 1;
	u32                          num_points = 0;
	float                        grid_scale = 1.0f;

	struct AssignedCell {
		Int3 ipos;
		u32  cell_id;
	};

	void setGridScale(float new_grid_scale) {
		grid_scale = new_grid_scale;
	}

	void setPoints(AssignedCell* __restrict assigned_cells, u32 num_vtxs) {
		assignCells(assigned_cells, num_vtxs);
		sortCells();
		findRanges();
	}

	template< typename Fn >
	void sortParticles(u32 first, u32 last, Fn fn) {
		PROFILE_SCOPED_NAMED("sortParticles");
		for (u32 gid = first; gid < last; ++gid) {
			const CellsPerVertex& cell_per_vertex = cells_per_vertex[gid];
			const CellInfo& cell_info = cells_info[cell_per_vertex.cell_id];
			assert(cell_per_vertex.idx_in_cell < last);
			u32 out_idx = cell_info.first + cell_per_vertex.idx_in_cell;
			// Notify the host we want to set particles gid at out_idx
			fn(gid, out_idx);
		}
	}

	struct CellInfo {
		u32  tag = 0;
		u32  num_particles = 0;
		u32  range_idx = 0;
		u32  first = 0;
		Int3 coords;
	};
	std::vector<CellInfo>  cells_info;

	struct CellsPerVertex {
		u32 cell_id = 0;
		u32 idx_in_cell = 0;
	};
	std::vector<CellsPerVertex>  cells_per_vertex;

	struct Range {
		u32 first;
		u32 last;
	};

	struct CellRange {
		u32   cell_id = 0;
		Range range;
	};

	std::vector< CellRange > cells_ranges;

	struct NearRanges {
		constexpr static int max_ranges = 3 * 3 * 3;
		u32   n = 0;
		Range ranges[max_ranges];
	};

	void collectRanges(NearRanges& near_ranges, u32 cell_id) const {
		//PROFILE_SCOPED_NAMED("Ranges");
		const CellInfo& cell_info = cells_info[cell_id];
		const auto& i_grid = cell_info.coords;
		u32 n = 0;

		Int3 j_grid = i_grid;
		for (int iz = -1; iz < 2; ++iz) {
			j_grid.z = i_grid.z + iz;
			for (int iy = -1; iy < 2; ++iy) {
				j_grid.y = i_grid.y + iy;
				for (int ix = -1; ix < 2; ++ix) {
					j_grid.x = i_grid.x + ix;

					const CellInfo* cell_j = nullptr;
					// Get the neighbour cell_id, rehashing the integer coords
					u32 jcell_id = gridHash(j_grid);
					
					while (true) {
						cell_j = &cells_info[jcell_id];
						// If the cell is not used, fine, otherwise the coord must match
						if ((cell_j->tag != current_tag) || (cell_j->coords == j_grid))
							break;
						// or it means we need to find the next cell (open address hash)
						jcell_id = (jcell_id + 1) & hash_mask;
					}

					// Confirm again the cell contains data in this frame
					if (cell_j->tag != current_tag)
						continue;

					// Keep the range
					const CellRange& cell_range_j = cells_ranges[cell_j->range_idx];
					near_ranges.ranges[n] = cell_range_j.range;
					n += 1;
				}
			}
		}
		near_ranges.n = n;
	}

	template< typename Fn >
	void onEachParticleInCell( const CellRange& range, Fn fn ) {
		//PROFILE_SCOPED_NAMED("Cell");
		NearRanges near_ranges;
		collectRanges(near_ranges, range.cell_id);

		// For each particle in cell 
		for( u32 i = range.range.first; i != range.range.last; ++i ) {
			fn(i, near_ranges);
		}
	}

	// Used for debug
	template< typename Fn>
	void onEachNeighbourOfParticle( const NearRanges& near_ranges, Fn fn ) {
		// For all valid ranges
		for (u32 k = 0; k < near_ranges.n; ++k) {
			u32 j = near_ranges.ranges[k].first;
			u32 last_j = near_ranges.ranges[k].last;
			while (j < last_j) {
				fn(j);
				++j;
			}
		}
	}

	template< typename Fn>
	void onEachCell(Fn fn) {
		for (const CellRange& range : cells_ranges) {
			onEachParticleInCell(range, fn);
		}
	}

	VEC3 getCellCoords(Int3 gridPos) const {
		return VEC3(gridPos.x / grid_scale, gridPos.y / grid_scale, gridPos.z / grid_scale);
	}

	Int3 gridCoords(VEC3 p) const {
		const VEC3 d = (p) * grid_scale;
		return Int3( (u32)floorf(d.x), (u32)floorf(d.y), (u32)floorf(d.z) );
	}

	u32 gridHash(Int3 gridPos) const {
		return (abs(((gridPos.y * 689287499) ^ (gridPos.z * 283923481) ^ ( gridPos.x * 83492791 )) & 0x7fffffff)) & hash_mask;
	}

	u32 hashOfCoord(VEC3 p) const {
		return gridHash(gridCoords(p));
	}

	u32 num_collisions = 0;

private:
	u32 current_tag = 0;

	void assignCells(AssignedCell* __restrict assigned_cells, u32 num_vtxs) {
		PROFILE_SCOPED_NAMED("assignCells");

		num_collisions = 0;
		reserve(num_vtxs);

		cells_ranges.clear();
		current_tag++;

		for (u32 gid = 0; gid < num_vtxs; ++gid) {
			Int3 ipos = assigned_cells[gid].ipos;
			u32  cell_id = assigned_cells[gid].cell_id;

			CellInfo* cell_info = nullptr;
			while (true) {
				cell_info = &cells_info[ cell_id ];
				if( cell_info->tag != current_tag ) {
					cell_info->tag = current_tag;
					cell_info->num_particles = 0;
					cell_info->range_idx = (u32) cells_ranges.size();
					cell_info->coords = ipos;
					cells_ranges.push_back( { cell_id } );
				}
				else {
					// Confirm there is no hash collision for this position, otherwise take the next cell
					if (ipos == cell_info->coords)
						break;
					cell_id = ( cell_id + 1 ) & hash_mask;
					++num_collisions;
				}
			}
			assert(cell_info->range_idx < cells_ranges.size());
			CellsPerVertex& cell_per_vertex = cells_per_vertex[gid];
			cell_per_vertex.idx_in_cell = cell_info->num_particles;
			cell_per_vertex.cell_id = cell_id;

			++cell_info->num_particles;
		}

	}

	void findRanges() {
		PROFILE_SCOPED_NAMED("findRanges");
		u32 acc = 0;
		for( CellRange& range : cells_ranges ) {
			CellInfo& cell_info = cells_info[ range.cell_id ];
			assert( cell_info.tag == current_tag );
			assert( cell_info.num_particles > 0 );
			// Save the acc into the cell_info
			cell_info.first = acc;
			range.range.first = acc;
			acc += cell_info.num_particles;
			range.range.last = acc;
		}
	}

	void reserve(u32 in_num_points) {
		num_points = in_num_points;
		cells_per_vertex.resize(num_points);
		cells_info.resize(num_cells);
	}

	static uint32_t morton3D(uint32_t x, uint32_t y, uint32_t z) {
		x = (x | (x << 16)) & 0x030000FF;
		x = (x | (x << 8)) & 0x0300F00F;
		x = (x | (x << 4)) & 0x030C30C3;
		x = (x | (x << 2)) & 0x09249249;

		y = (y | (y << 16)) & 0x030000FF;
		y = (y | (y << 8)) & 0x0300F00F;
		y = (y | (y << 4)) & 0x030C30C3;
		y = (y | (y << 2)) & 0x09249249;

		z = (z | (z << 16)) & 0x030000FF;
		z = (z | (z << 8)) & 0x0300F00F;
		z = (z | (z << 4)) & 0x030C30C3;
		z = (z | (z << 2)) & 0x09249249;

		return x | (y << 1) | (z << 2);
	}

  void sortCells() {
    PROFILE_SCOPED_NAMED("sortCells");
    std::sort(cells_ranges.begin(), cells_ranges.end(), [&](const CellRange& a, const CellRange& b) {
			CellInfo& ca = cells_info[a.cell_id];
			CellInfo& cb = cells_info[b.cell_id];
			// No noticiable performance win. 
			//return morton3D(ca.coords.x, ca.coords.y, ca.coords.z) < morton3D(cb.coords.x, cb.coords.y, cb.coords.z);
			
			// Sort by height, x, then z
			if ( ca.coords.y != cb.coords.y )
				return ca.coords.y < cb.coords.y;
			if (ca.coords.x != cb.coords.x)
				return ca.coords.x < cb.coords.x;
			return ca.coords.z < cb.coords.z;
			});

		int idx = 0;
    for (auto& cell : cells_ranges)
			cells_info[cell.cell_id].range_idx = idx++;
  }

};


