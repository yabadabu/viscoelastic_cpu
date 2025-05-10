#pragma once

#include "cpu_spatial_subdivision.h"
#include "particles_vec.h"
#include "geometry/sdf/sdf.h"
#include "thread_pool.h"

struct ViscoelasticSim {

  enum eSection {
    SpatialHash,
    VelocitiesUpdate,
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
  int                     max_particles = 65536;
  float                   max_speed = 5.0;

  float                   masses[4] = { 1.0f, 2.0f, 3.0f, 4.0f };

  bool                    in_2d = false;
  bool                    attract = false;
  bool                    repel = false;
  bool                    drain = false;
  bool                    emit = false;
  bool                    drag = false;

  VEC3                    mouse = VEC3::zero;
  VEC3                    mouse_prev = VEC3::zero;
  float                   world_scale = 100.0f;
  int                     num_substeps = 1;

  int                     debug_particle = -1;

  bool using_parallel = false;

  static constexpr int num_threads = 12;
  ThreadPool* pool = nullptr;

  std::vector< CPUSpatialSubdivision::AssignedCell > assigned_cells;

  void init();

  void addParticle(VEC3 pos, VEC3 vel);

  void updateSpatialHash();
  void resolveCollisions(float dt, int start, int end);
  void processRange(float dt, const CPUSpatialSubdivision::CellRange& range, const ParticlesVec& __restrict ppos, ParticlesVec* __restrict deltas);
  void updateStep(float dt);
  void update(float dt);

  template< typename Fn >
  void runInParallel(int num_jobs, int num_splits, Fn fn) {
    PROFILE_SCOPED_NAMED("runInParallel");
    int chunk_size = (num_jobs + num_splits - 1) / num_splits;
    std::vector<std::future<void>> jobs;
    for (int job_id = 0; job_id < num_splits; ++job_id) {
      int start = job_id * chunk_size;
      int end = std::min(start + chunk_size, num_jobs);
      jobs.emplace_back(pool->enqueue([&, start, end, job_id]() {
        PROFILE_SCOPED_NAMED("C");
        fn(start, end, job_id);
        }));
    }
    for (auto& job : jobs)
      job.get();
  }

  void doubleDensityRelaxationPara(float dt, ThreadPool& pool) {
    int num_jobs = (int)spatial_hash.cells_ranges.size();
    runInParallel(num_jobs, num_threads * 3, [&](int start, int end, int job_id) {
      for (int i = start; i < end; ++i)
        processRange(dt, spatial_hash.cells_ranges[i], particles_frozen_pos, &particles_pos);
      });
  }

  void doubleDensityRelaxation(float dt) {
    PROFILE_SCOPED_NAMED("doubleDensityRelaxation");
    for (auto& range : spatial_hash.cells_ranges)
      processRange(dt, range, particles_frozen_pos, &particles_pos);
  }

  void saveTime(eSection section_id, TTimer& tm) {
    times[ section_id ] = times[section_id] * 0.9f + tm.elapsed() * 0.1f;
  }
};
