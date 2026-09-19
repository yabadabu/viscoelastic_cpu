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

	struct UniqueCell {
		Int3 ipos;
		u32  initial_cell_id;
		u32  num_particles;
		u32  source_idx;
	};

	void setGridScale(float new_grid_scale) {
		grid_scale = new_grid_scale;
	}

	void setPoints(AssignedCell* __restrict assigned_cells, u32 num_vtxs) {
		using_compact_lookup = false;
		assignCells(assigned_cells, num_vtxs);
		sortCells();
		findRanges();
	}

	// GPU-style compact representation used by the parallel builder. Cells are
	// already spatially sorted, so ranges can be emitted directly. A compact
	// hash -> candidate-cell table replaces serial open-address insertion while
	// still resolving hash collisions by comparing the exact grid coordinate.
	void setCompactCells(
		const UniqueCell* __restrict sorted_unique_cells,
		u32 num_unique_cells,
		u32 num_vtxs
	) {
		PROFILE_SCOPED_NAMED("buildCompactSpatialIndex");
		using_compact_lookup = true;
		num_collisions = 0;
		reserve(num_vtxs);
		cells_ranges.resize(num_unique_cells);
		current_tag++;

		u32 particle_offset = 0;
		for (u32 unique_idx = 0; unique_idx < num_unique_cells; ++unique_idx) {
			const UniqueCell& unique_cell = sorted_unique_cells[unique_idx];
			CellInfo& cell_info = cells_info[unique_idx];
			cell_info.tag = current_tag;
			cell_info.num_particles = unique_cell.num_particles;
			cell_info.range_idx = unique_idx;
			cell_info.first = particle_offset;
			cell_info.coords = unique_cell.ipos;
			cells_ranges[unique_idx] = {
				unique_idx,
				{ particle_offset, particle_offset + unique_cell.num_particles }
			};
			particle_offset += unique_cell.num_particles;
		}
		assert(particle_offset == num_vtxs);

		// Count, prefix-scan and scatter the spatially ordered cell ids into
		// their original hash buckets. Hash collisions become short contiguous
		// candidate lists instead of open-addressed probe chains.
		compact_hash_offsets.assign((size_t)num_cells + 1, 0);
		for (u32 unique_idx = 0; unique_idx < num_unique_cells; ++unique_idx)
			++compact_hash_offsets[sorted_unique_cells[unique_idx].initial_cell_id + 1];
		for (u32 hash = 0; hash < num_cells; ++hash)
			compact_hash_offsets[hash + 1] += compact_hash_offsets[hash];

		compact_hash_cell_ids.resize(num_unique_cells);
		compact_hash_cursors.assign(compact_hash_offsets.begin(), compact_hash_offsets.end() - 1);
		for (u32 unique_idx = 0; unique_idx < num_unique_cells; ++unique_idx) {
			const UniqueCell& unique_cell = sorted_unique_cells[unique_idx];
			u32& cursor = compact_hash_cursors[unique_cell.initial_cell_id];
			const u32 candidates_before = cursor - compact_hash_offsets[unique_cell.initial_cell_id];
			num_collisions += candidates_before * unique_cell.num_particles;
			compact_hash_cell_ids[cursor++] = unique_idx;
		}
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

		// gridHash is separable into one XOR component per axis. Compute the
		// three possible values for each axis once instead of performing three
		// multiplications for every one of the 27 neighbours.
		u32 hash_x[3];
		u32 hash_y[3];
		u32 hash_z[3];
		for (int offset = -1; offset <= 1; ++offset) {
			const u32 unsigned_offset = static_cast<u32>(offset);
			hash_x[offset + 1] = (static_cast<u32>(i_grid.x) + unsigned_offset) * 83492791u;
			hash_y[offset + 1] = (static_cast<u32>(i_grid.y) + unsigned_offset) * 689287499u;
			hash_z[offset + 1] = (static_cast<u32>(i_grid.z) + unsigned_offset) * 283923481u;
		}

		Int3 j_grid = i_grid;
		// cells_ranges and the particle arrays are ordered by y, x, then z.
		// Visit neighbours in the same order to keep their particle ranges
		// streaming through cache and to make adjacent ranges coalescible.
		for (int iy = -1; iy < 2; ++iy) {
			j_grid.y = i_grid.y + iy;
			for (int ix = -1; ix < 2; ++ix) {
				j_grid.x = i_grid.x + ix;
				for (int iz = -1; iz < 2; ++iz) {
					j_grid.z = i_grid.z + iz;

					const CellInfo* cell_j = nullptr;
					// Get the neighbour cell_id from the precomputed axis hashes.
					u32 jcell_id = (hash_y[iy + 1] ^ hash_z[iz + 1] ^ hash_x[ix + 1]) & hash_mask;

					if (using_compact_lookup) {
						const u32 first_candidate = compact_hash_offsets[jcell_id];
						const u32 last_candidate = compact_hash_offsets[jcell_id + 1];
						for (u32 candidate = first_candidate; candidate < last_candidate; ++candidate) {
							const CellInfo& candidate_info = cells_info[compact_hash_cell_ids[candidate]];
							if (candidate_info.coords == j_grid) {
								cell_j = &candidate_info;
								break;
							}
						}
						if (cell_j == nullptr)
							continue;
					}
					else {
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
					}

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
		return VEC3(gridPos.x / grid_scale, gridPos.y / grid_scale, gridPos.z / grid_scale);
	}

	Int3 gridCoords(VEC3 p) const {
		const VEC3 d = (p) * grid_scale;
		return Int3( (u32)floorf(d.x), (u32)floorf(d.y), (u32)floorf(d.z) );
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
	bool using_compact_lookup = false;
	std::vector<u32> compact_hash_offsets;
	std::vector<u32> compact_hash_cell_ids;
	std::vector<u32> compact_hash_cursors;

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


