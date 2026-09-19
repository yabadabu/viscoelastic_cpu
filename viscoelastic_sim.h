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
  bool overlap_cache_and_prediction = true;
  ThreadPool* pool = nullptr;
  std::vector<ParticlesVec> relaxation_worker_deltas;
  std::vector<CPUSpatialSubdivision::NearRanges> relaxation_near_ranges;

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
  void resolveCollisions(float dt, int start, int end);
  void processRange(float dt, const CPUSpatialSubdivision::CellRange& range, const CPUSpatialSubdivision::NearRanges& near_ranges, const ParticlesVec& __restrict ppos, ParticlesVec* __restrict out_deltas);
  void updateStep(float dt);
  void update(float dt);
  void doubleDensityRelaxationPara(float dt, ThreadPool& pool);
  void doubleDensityRelaxation(float dt);
  void cacheRanges();
  void cacheRangesAndPredict(float dt);
  void updatePredictedPositions(float dt);
  void updatePredictedPositionsRange(float dt, int start, int end);
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
