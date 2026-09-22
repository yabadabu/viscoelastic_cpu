#pragma once

#include "cpu_spatial_subdivision.h"
#include "particles_vec.h"
#include "geometry/sdf/sdf.h"
#include "thread_pool.h"

struct ViscoelasticSim {

  enum eSection {
    SpatialHash,
    CacheRanges,
    PredictPositions,
    Relaxation,
    VelocitiesFromPositions,
    Collisions,
    Render,
    Update,
    NumSections
  };
  double times[eSection::NumSections] = { 0.0f };

  struct Material {
    float       rest_density = 4.0f;
    float       stiffness = 0.5f;
    float       near_stiffness = 0.5f;
    float       kernel_radius = 20;
    float       point_size = 5.0f;
    float       dt = 1.0f;
    VEC3        gravity = VEC3(0, -0.5f, 0);
  };

  ParticlesVec   particles_pos;
  ParticlesVec   particles_prev_pos;
  ParticlesVec   particles_frozen_pos;
  ParticlesVec   particles_vels;
  unsigned char* particles_type = nullptr;

  ParticlesVec   aux_particles_pos;
  ParticlesVec   aux_particles_prev_pos;
  ParticlesVec   aux_particles_vels;
  unsigned char* aux_particles_type = nullptr;

  Material                mat;
  CPUSpatialSubdivision   spatial_hash;

  SDF::sdFunc             sdf;
  float                   friction = 2.0f;
  int                     num_particles = 0;
  int                     max_particles = 128 * 1024;
  float                   max_speed = 5.0;

  float                   masses[4] = { 1.0f, 2.0f, 3.0f, 4.0f };

  bool                    in_2d = false;
  bool                    attract = false;
  bool                    repel = false;
  bool                    emit = false;
  bool                    using_parallel = false;

  VEC3                    interact_point = VEC3::zero;
  VEC3                    interact_dir = VEC3::axis_y;
  float                   interact_rad = 80.0;

  float                   world_scale = 100.0f;
  int                     num_substeps = 1;

  int                     debug_particle = -1;

  int num_threads = 12;
  int sort_jobs_per_thread = 4;
  int cache_jobs_per_thread = 6;
  int prediction_jobs = 8;
  int relaxation_jobs_per_thread = 12;
  int relaxation_reduce_jobs_per_thread = 4;
  bool use_parallel_spatial_index = false;
  bool use_hierarchical_spatial_index = false;
  bool use_bounded_xy_spatial_index = false;
  float spatial_xy_bound_world_min = -20.0f;
  float spatial_xy_bound_world_max = 20.0f;
  int spatial_hierarchy_macro_side = 4;
  bool spatial_hierarchy_xy_columns = false;
  bool sort_hierarchical_neighbour_ranges = false;
  int spatial_index_buckets = 64;
  bool overlap_cache_and_prediction = true;
  ThreadPool* pool = nullptr;
  std::vector<ParticlesVec> relaxation_worker_deltas;
  std::vector<CPUSpatialSubdivision::NearRanges> relaxation_near_ranges;

  // Scratch storage for the optional histogram/prefix-scan/scatter spatial
  // index builder. It is retained between frames to avoid allocator traffic.
  std::vector<uint32_t> spatial_bucket_counts;
  std::vector<uint32_t> spatial_bucket_offsets;
  std::vector<uint32_t> spatial_bucket_starts;
  std::vector<uint32_t> spatial_bucket_particle_ids;
  std::vector<uint32_t> spatial_bucket_unique_counts;
  std::vector<uint32_t> spatial_bucket_unique_offsets;
  std::vector<uint32_t> spatial_bucket_hash_offsets;
  std::vector<uint32_t> spatial_partition_unique_offsets;
  std::vector<CPUSpatialSubdivision::Int3> spatial_local_hash_coords;
  std::vector<uint32_t> spatial_local_hash_unique_indices;
  std::vector<uint32_t> spatial_particle_unique_indices;
  std::vector<uint32_t> spatial_particle_indices_in_cell;
  std::vector<CPUSpatialSubdivision::UniqueCell> spatial_provisional_unique_cells;
  std::vector<CPUSpatialSubdivision::UniqueCell> spatial_unique_cells;
  std::vector<uint32_t> spatial_source_unique_indices;

  // Scratch storage for the bounded exact-XY-column builder. Histogram rows
  // belong to stable particle partitions (not physical worker identities), so
  // the scatter pass can reuse each row as a private cursor array.
  std::vector<uint32_t> bounded_xy_partition_histograms;
  std::vector<std::vector<uint32_t>> bounded_xy_partition_touched_columns;
  std::vector<uint32_t> bounded_xy_particle_columns;
  std::vector<uint32_t> bounded_xy_column_counts;
  std::vector<uint32_t> bounded_xy_column_particle_offsets;
  std::vector<uint32_t> bounded_xy_column_particle_ids;
  std::vector<uint32_t> bounded_xy_occupied_columns;
  std::vector<uint32_t> bounded_xy_column_unique_counts;
  std::vector<uint32_t> bounded_xy_column_cell_offsets;
  std::vector<uint8_t> bounded_xy_partition_out_of_bounds;

  struct HierarchyMacro {
    CPUSpatialSubdivision::Int3 coords;
    uint32_t particle_count = 0;
    uint32_t source_idx = 0;
    uint32_t particle_first = 0;
    uint32_t cell_first = 0;
  };
  std::vector<CPUSpatialSubdivision::Int3> hierarchy_particle_macro_coords;
  std::vector<uint16_t> hierarchy_particle_local_ids;
  std::vector<HierarchyMacro> hierarchy_provisional_macros;
  std::vector<HierarchyMacro> hierarchy_macros;
  std::vector<uint32_t> hierarchy_source_macro_indices;
  std::vector<uint32_t> hierarchy_macro_particle_offsets;
  std::vector<uint32_t> hierarchy_macro_particle_cursors;
  std::vector<uint32_t> hierarchy_macro_particle_ids;
  std::vector<uint32_t> hierarchy_local_cell_counts;
  std::vector<uint32_t> hierarchy_local_cell_ids;
  std::vector<uint32_t> hierarchy_local_cell_cursors;
  std::vector<uint32_t> hierarchy_macro_occupied_cell_counts;
  std::vector<int> hierarchy_macro_min_z;
  std::vector<uint32_t> hierarchy_macro_z_spans;
  std::vector<uint32_t> hierarchy_macro_cell_table_offsets;
  std::vector<uint8_t> hierarchy_macro_sparse_fallbacks;

  struct SpatialHierarchyAudit {
    struct MacroStats {
      int side = 0;
      int occupied_macros = 0;
      int occupied_small_cells = 0;
      float macros_per_worker = 0.0f;
      float average_particles = 0.0f;
      int p95_particles = 0;
      int max_particles = 0;
      float largest_particle_percent = 0.0f;
      float average_occupied_cells = 0.0f;
      int p95_occupied_cells = 0;
      int max_occupied_cells = 0;
      float average_local_table_slots = 0.0f;
      float average_local_table_occupancy_percent = 0.0f;
    };

    bool requested = false;
    bool valid = false;
    bool completed_this_update = false;
    bool xy_columns = false;
    int num_particles = 0;
    MacroStats configurations[4];
  } spatial_hierarchy_audit;

  struct NeighbourRangeAudit {
    bool requested = false;
    bool valid = false;
    bool completed_this_update = false;
    bool hierarchical = false;
    bool xy_columns = false;
    bool sorted_by_particle_offset = false;
    int macro_side = 0;
    int num_cells = 0;
    uint64_t ranges_before = 0;
    uint64_t ranges_after = 0;
    int max_ranges_before = 0;
    int max_ranges_after = 0;
    float average_ranges_before = 0.0f;
    float average_ranges_after = 0.0f;
  } neighbour_range_audit;
  std::vector<uint8_t> neighbour_range_counts_before_sort;

  struct RelaxationAudit {
    bool requested = false;
    bool valid = false;
    bool completed_this_update = false;
    int num_workers = 0;
    int num_particles = 0;
    int range_size = 64;
    uint64_t total_delta_slots = 0;
    uint64_t nonzero_delta_slots = 0;
    uint64_t total_simd_blocks = 0;
    uint64_t active_simd_blocks = 0;
    uint64_t total_range_worker_pairs = 0;
    uint64_t active_range_worker_pairs = 0;
    uint64_t minmax_delta_slots = 0;
    uint64_t simd_aligned_minmax_delta_slots = 0;
    int active_workers = 0;
    float average_workers_per_particle = 0.0f;
    int max_workers_per_particle = 0;
    float average_workers_per_range = 0.0f;
    int min_workers_per_range = 0;
    int max_workers_per_range = 0;
    uint64_t neighbour_candidates_available = 0;
    uint64_t neighbour_candidates_checked = 0;
    uint64_t neighbour_candidates_accepted = 0;
    uint64_t neighbour_candidates_rejected = 0;
    uint64_t neighbour_candidates_discarded_by_cap = 0;
    uint64_t neighbour_candidates_skipped_by_cap = 0;
    uint64_t neighbour_simd_blocks_checked = 0;
    uint64_t neighbour_simd_blocks_active = 0;
    uint64_t particles_at_neighbour_cap = 0;
    std::vector<float> workers_per_range;
    std::vector<float> worker_range_coverage_percent;
    std::vector<int> worker_min_touched_particle;
    std::vector<int> worker_max_touched_particle;
    std::vector<uint64_t> worker_nonzero_delta_slots;
  } relaxation_audit;

  void setNumThreads(int new_num_threads);

  std::vector< CPUSpatialSubdivision::AssignedCell > assigned_cells;

  void init();

  void addParticle(VEC3 pos, VEC3 vel, uint8_t particle_type);
  void removeParticle(int particle_id);
  void removeParticles(std::vector<int>& particles_to_remove);
  void getParticleIDsNear(std::vector<int>& out_ids, VEC3 ref_point, float rad) const;

  void updateSpatialHash();
  void assignCellsParallel();
  void assignCellsHierarchical();
  bool assignCellsBoundedXY();
  void resolveCollisions(float dt, int start, int end);
  void processRange(float dt, const CPUSpatialSubdivision::CellRange& range, const CPUSpatialSubdivision::NearRanges& near_ranges, const ParticlesVec& __restrict ppos, ParticlesVec* __restrict out_deltas);
  void updateStep(float dt);
  void update(float dt);
  void doubleDensityRelaxationPara(float dt, ThreadPool& pool);
  void doubleDensityRelaxation(float dt);
  void cacheRanges();
  void cacheNearRanges(int cell_idx, bool capture_audit);
  void cacheDirectColumnRanges(uint32_t column, bool capture_audit);
  void finishNeighbourRangeAudit();
  void cacheRangesAndPredict(float dt);
  void updatePredictedPositions(float dt);
  void updatePredictedPositionsRange(float dt, int start, int end);
  void captureSpatialHierarchyAudit();
  void captureRelaxationAudit();

  template< typename Fn >
  void runInParallel(int num_jobs, int num_splits, Fn fn) {
    PROFILE_SCOPED_NAMED("runInParallel");
    if (num_jobs <= 0)
      return;
    num_splits = std::min(num_splits, num_jobs);
    int chunk_size = (num_jobs + num_splits - 1) / num_splits;
    pool->dispatch(num_splits, [&](int job_id) {
      int start = job_id * chunk_size;
      int end = std::min(start + chunk_size, num_jobs);
      PROFILE_SCOPED_NAMED("C");
      fn(start, end, job_id);
      });
  }

  void saveTime(eSection section_id, TTimer& tm) {
    times[ section_id ] = times[section_id] * 0.9f + tm.elapsed() * 0.1f;
  }
};
