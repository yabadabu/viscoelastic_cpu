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
  bool                    emit = false;
  bool                    using_parallel = false;

  VEC3                    interact_point = VEC3::zero;
  VEC3                    interact_dir = VEC3::axis_y;
  float                   interact_rad = 80.0;

  float                   world_scale = 100.0f;
  int                     num_substeps = 1;

  int                     debug_particle = -1;

  int num_threads = 12;
  ThreadPool* pool = nullptr;

  void setNumThreads(int new_num_threads);

  std::vector< CPUSpatialSubdivision::AssignedCell > assigned_cells;

  void init();

  void addParticle(VEC3 pos, VEC3 vel, uint8_t particle_type);
  void removeParticle(int particle_id);
  void removeParticles(std::vector<int>& particles_to_remove);
  void getParticleIDsNear(std::vector<int>& out_ids, VEC3 ref_point, float rad) const;

  void updateSpatialHash();
  void resolveCollisions(float dt, int start, int end);
  void processRange(float dt, const CPUSpatialSubdivision::CellRange& range, const ParticlesVec& __restrict ppos, ParticlesVec* __restrict deltas);
  void updateStep(float dt);
  void update(float dt);
  void doubleDensityRelaxationPara(float dt, ThreadPool& pool);
  void doubleDensityRelaxation(float dt);

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

  void saveTime(eSection section_id, TTimer& tm) {
    times[ section_id ] = times[section_id] * 0.9f + tm.elapsed() * 0.1f;
  }
};
