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
	static constexpr int         max_neighbour_cell_radius_xy = 1;
	static constexpr int         max_neighbour_cell_radius_z = 2;
	u32                          num_points = 0;
	float                        grid_scale_xy = 1.0f;
	float                        grid_scale_z = 1.0f;
	int                          neighbour_cell_radius_xy = 1;
	int                          neighbour_cell_radius_z = 1;

	struct AssignedCell {
		Int3 ipos;
		u32  cell_id;
	};

	void setGridScale(
		float new_grid_scale_xy,
		float new_grid_scale_z,
		int new_neighbour_cell_radius_xy,
		int new_neighbour_cell_radius_z
	) {
		assert(new_grid_scale_xy > 0.0f && new_grid_scale_z > 0.0f);
		assert(new_neighbour_cell_radius_xy >= 1 &&
			new_neighbour_cell_radius_xy <= max_neighbour_cell_radius_xy);
		assert(new_neighbour_cell_radius_z >= 1 &&
			new_neighbour_cell_radius_z <= max_neighbour_cell_radius_z);
		grid_scale_xy = new_grid_scale_xy;
		grid_scale_z = new_grid_scale_z;
		neighbour_cell_radius_xy = new_neighbour_cell_radius_xy;
		neighbour_cell_radius_z = new_neighbour_cell_radius_z;
	}

	float getGridScaleXY() const {
		return grid_scale_xy;
	}

	int getNeighbourCellRadiusXY() const {
		return neighbour_cell_radius_xy;
	}

	int getNeighbourCellRadiusZ() const {
		return neighbour_cell_radius_z;
	}

	VEC3 getCellSize() const {
		return VEC3(
			1.0f / grid_scale_xy,
			1.0f / grid_scale_xy,
			1.0f / grid_scale_z);
	}

	void setPoints(AssignedCell* __restrict assigned_cells, u32 num_vtxs) {
		using_direct_column_lookup = false;
		assignCells(assigned_cells, num_vtxs);
		sortCells();
		findRanges();
	}

	struct DirectColumn {
		u32 tag = 0;
		u32 first_cell = 0;
		u32 num_cells = 0;
	};

	// The bounded XY builder emits cells ordered by column, then Z.
	// Store only the cell interval for each XY column; Z remains unbounded and
	// is resolved with a binary search over that short sorted interval.
	void prepareDirectColumns(
		int min_x,
		int min_y,
		int max_x,
		int max_y,
		u32 num_unique_cells,
		u32 num_vtxs
	) {
		assert(min_x <= max_x && min_y <= max_y);
		using_direct_column_lookup = false;
		num_collisions = 0;
		reserve(num_vtxs);
		cells_ranges.resize(num_unique_cells);
		current_tag++;
		direct_min_x = min_x;
		direct_min_y = min_y;
		direct_max_x = max_x;
		direct_max_y = max_y;
		direct_width = (u32)((int64_t)max_x - min_x + 1);
		direct_columns.resize(
			(size_t)direct_width * ((int64_t)max_y - min_y + 1));
		using_direct_column_lookup = true;
	}

	void setDirectColumn(int x, int y, u32 first_cell, u32 cell_count) {
		assert(using_direct_column_lookup);
		assert(x >= direct_min_x && x <= direct_max_x);
		assert(y >= direct_min_y && y <= direct_max_y);
		assert((uint64_t)first_cell + cell_count <= cells_ranges.size());
		DirectColumn& column = direct_columns[directColumnIndex(x, y)];
		column.tag = current_tag;
		column.first_cell = first_cell;
		column.num_cells = cell_count;
	}

	// Return the current frame's exact-Z-sorted cell interval for an XY
	// column. The bounded cache builder uses this to merge-walk neighbouring
	// columns instead of performing a binary search for every target cell.
	const DirectColumn* findDirectColumn(int x, int y) const {
		if (!using_direct_column_lookup ||
			x < direct_min_x || x > direct_max_x ||
			y < direct_min_y || y > direct_max_y)
			return nullptr;
		const DirectColumn& column = direct_columns[directColumnIndex(x, y)];
		return column.tag == current_tag ? &column : nullptr;
	}

	void setDirectCellMetadata(
		u32 unique_idx,
		const Int3& coords,
		u32 num_particles,
		u32 particle_offset
	) {
		CellInfo& cell_info = cells_info[unique_idx];
		cell_info.tag = current_tag;
		cell_info.num_particles = num_particles;
		cell_info.range_idx = unique_idx;
		cell_info.first = particle_offset;
		cell_info.coords = coords;
		cells_ranges[unique_idx] = {
			unique_idx,
			{ particle_offset, particle_offset + num_particles }
		};
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
		// Cells are ordered by Y, X, Z, so all relevant Z cells in each
		// neighbouring XY column form one contiguous particle range. Both the
		// R and half-Z modes need only 3x3 neighbouring XY columns.
		// For R-sized cells, a tested 19-cell stencil removed about 29% of
		// candidates, but its missing corner interactions caused persistent
		// vibration after the fluid should have settled.
		constexpr static int max_ranges =
			(2 * max_neighbour_cell_radius_xy + 1) *
			(2 * max_neighbour_cell_radius_xy + 1);
		u32   n = 0;
		Range ranges[max_ranges];
	};

	void collectRanges(NearRanges& near_ranges, u32 cell_id) const {
		//PROFILE_SCOPED_NAMED("Ranges");
		const CellInfo& cell_info = cells_info[cell_id];
		const auto& i_grid = cell_info.coords;
		const int xy_radius = neighbour_cell_radius_xy;
		const int z_radius = neighbour_cell_radius_z;
		u32 n = 0;

		if (using_direct_column_lookup) {
			const int first_y = std::max(i_grid.y - xy_radius, direct_min_y);
			const int last_y = std::min(i_grid.y + xy_radius, direct_max_y);
			const int first_x = std::max(i_grid.x - xy_radius, direct_min_x);
			const int last_x = std::min(i_grid.x + xy_radius, direct_max_x);
			const int first_z = i_grid.z - z_radius;
			const int last_z = i_grid.z + z_radius;

			for (int y = first_y; y <= last_y; ++y) {
				for (int x = first_x; x <= last_x; ++x) {
					const DirectColumn& column =
						direct_columns[directColumnIndex(x, y)];
					if (column.tag != current_tag)
						continue;

					u32 first = column.first_cell;
					u32 last = first + column.num_cells;
					// Find the first cell whose Z can be a neighbour.
					while (first < last) {
						const u32 middle = first + (last - first) / 2;
						if (cells_info[middle].coords.z < first_z)
							first = middle + 1;
						else
							last = middle;
					}

					const u32 column_end =
						column.first_cell + column.num_cells;
					for (u32 neighbour_id = first;
						 neighbour_id < column_end &&
						 cells_info[neighbour_id].coords.z <= last_z;
						 ++neighbour_id) {
						const CellInfo& neighbour = cells_info[neighbour_id];
						const Range neighbour_range = {
							neighbour.first,
							neighbour.first + neighbour.num_particles
						};
						if (n > 0 &&
							near_ranges.ranges[n - 1].last == neighbour_range.first) {
							near_ranges.ranges[n - 1].last = neighbour_range.last;
						}
						else {
							assert(n < NearRanges::max_ranges);
							near_ranges.ranges[n] = neighbour_range;
							++n;
						}
					}
				}
			}
			near_ranges.n = n;
			return;
		}

		// gridHash is separable into one XOR component per axis. Compute the
		// possible values for each axis once instead of repeating three
		// multiplications for every neighbour.
		constexpr int max_xy_diameter =
			2 * max_neighbour_cell_radius_xy + 1;
		constexpr int max_z_diameter =
			2 * max_neighbour_cell_radius_z + 1;
		u32 hash_x[max_xy_diameter];
		u32 hash_y[max_xy_diameter];
		u32 hash_z[max_z_diameter];
		for (int offset = -xy_radius; offset <= xy_radius; ++offset) {
			const u32 unsigned_offset = static_cast<u32>(offset);
			const int hash_idx = offset + xy_radius;
			hash_x[hash_idx] = (static_cast<u32>(i_grid.x) + unsigned_offset) * 83492791u;
			hash_y[hash_idx] = (static_cast<u32>(i_grid.y) + unsigned_offset) * 689287499u;
		}
		for (int offset = -z_radius; offset <= z_radius; ++offset) {
			const u32 unsigned_offset = static_cast<u32>(offset);
			const int hash_idx = offset + z_radius;
			hash_z[hash_idx] =
				(static_cast<u32>(i_grid.z) + unsigned_offset) * 283923481u;
		}

		Int3 j_grid = i_grid;
		// cells_ranges and the particle arrays are ordered by y, x, then z.
		// Visit neighbours in the same order to keep their particle ranges
		// streaming through cache and to make adjacent ranges coalescible.
		for (int iy = -xy_radius; iy <= xy_radius; ++iy) {
			j_grid.y = i_grid.y + iy;
			for (int ix = -xy_radius; ix <= xy_radius; ++ix) {
				j_grid.x = i_grid.x + ix;
				for (int iz = -z_radius; iz <= z_radius; ++iz) {
					j_grid.z = i_grid.z + iz;

					const CellInfo* cell_j = nullptr;
					// Get the neighbour cell_id from the precomputed axis hashes.
					u32 jcell_id =
						(hash_y[iy + xy_radius] ^
						 hash_z[iz + z_radius] ^
						 hash_x[ix + xy_radius]) & hash_mask;

					while (true) {
						cell_j = &cells_info[jcell_id];
						// If the cell is not used, fine, otherwise the coord must match.
						if ((cell_j->tag != current_tag) || (cell_j->coords == j_grid))
							break;
						jcell_id = (jcell_id + 1) & hash_mask;
					}

					if (cell_j->tag != current_tag)
						continue;

					// Keep the range
					// Avoid a second dependent lookup through cells_ranges: findRanges
					// has already cached the same interval in this CellInfo.
					const Range neighbour_range = {
						cell_j->first,
						cell_j->first + cell_j->num_particles
					};
					if (n > 0 && near_ranges.ranges[n - 1].last == neighbour_range.first) {
						near_ranges.ranges[n - 1].last = neighbour_range.last;
					}
					else {
						assert(n < NearRanges::max_ranges);
						near_ranges.ranges[n] = neighbour_range;
						n += 1;
					}
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
		return VEC3(
			gridPos.x / grid_scale_xy,
			gridPos.y / grid_scale_xy,
			gridPos.z / grid_scale_z);
	}

	Int3 gridCoords(VEC3 p) const {
		// Keep negative coordinates negative. Converting a negative float directly
		// to uint32_t is outside the representable range and has undefined results;
		// the hash functions already convert the signed coordinate to uint32_t when
		// they intentionally need its two's-complement bit pattern.
		return Int3(
			(int)floorf(p.x * grid_scale_xy),
			(int)floorf(p.y * grid_scale_xy),
			(int)floorf(p.z * grid_scale_z));
	}

	u32 gridHash(Int3 gridPos) const {
		const u32 hash_y = static_cast<u32>(gridPos.y) * 689287499u;
		const u32 hash_z = static_cast<u32>(gridPos.z) * 283923481u;
		const u32 hash_x = static_cast<u32>(gridPos.x) * 83492791u;
		return (hash_y ^ hash_z ^ hash_x) & hash_mask;
	}

	u32 hashOfCoord(VEC3 p) const {
		return gridHash(gridCoords(p));
	}

	u32 num_collisions = 0;
	bool using_direct_column_lookup = false;
	std::vector<DirectColumn> direct_columns;

private:
	u32 current_tag = 0;
	int direct_min_x = 0;
	int direct_min_y = 0;
	int direct_max_x = -1;
	int direct_max_y = -1;
	u32 direct_width = 0;

	size_t directColumnIndex(int x, int y) const {
		return (size_t)(y - direct_min_y) * direct_width +
			(u32)(x - direct_min_x);
	}

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
		cells_info.resize(std::max<u32>(num_cells, in_num_points));
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


