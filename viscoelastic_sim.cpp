#include "platform.h"
#include "viscoelastic_sim.h"
#include <immintrin.h>

// for (int i = 0; i < num_particles; ++i)
//  pos.add(i, vel.get(i) * dt);
void simd_update_positions(
  ParticlesVec& pos, 
  const ParticlesVec& vel, 
  float dt, 
  int num_particles
) {
  const int simd_width = 8;
  const int simd_end = num_particles & ~(simd_width - 1); // round down to nearest multiple of 8

  __m256 dt_vec = _mm256_set1_ps(dt);

  for (int i = 0; i < simd_end; i += simd_width) {
    // Load position and velocity components
    __m256 px = _mm256_loadu_ps(&pos.x[i]);
    __m256 py = _mm256_loadu_ps(&pos.y[i]);
    __m256 pz = _mm256_loadu_ps(&pos.z[i]);

    __m256 vx = _mm256_loadu_ps(&vel.x[i]);
    __m256 vy = _mm256_loadu_ps(&vel.y[i]);
    __m256 vz = _mm256_loadu_ps(&vel.z[i]);

    // Multiply velocity by dt
    vx = _mm256_mul_ps(vx, dt_vec);
    vy = _mm256_mul_ps(vy, dt_vec);
    vz = _mm256_mul_ps(vz, dt_vec);

    // Add to positions
    px = _mm256_add_ps(px, vx);
    py = _mm256_add_ps(py, vy);
    pz = _mm256_add_ps(pz, vz);

    // Store back
    _mm256_storeu_ps(&pos.x[i], px);
    _mm256_storeu_ps(&pos.y[i], py);
    _mm256_storeu_ps(&pos.z[i], pz);
  }

  // Fallback scalar for remainder
  for (int i = simd_end; i < num_particles; ++i)
    pos.add(i, vel.get(i) * dt);
}

// 0.171ms -> 0.026ms
//for (int i = 0; i < num_particles; ++i) {
//  particles_vels.set(i, (particles_pos.get(i) - particles_prev_pos.get(i)) * inv_dt);
//  if (particles_vels.get(i).Length() > max_speed)
//    particles_vels.set(i, particles_vels.get(i).Normalized() * max_speed);
//}
void simd_update_velocities_clamped(
  ParticlesVec& vel,
  const ParticlesVec& pos,
  const ParticlesVec& prev,
  float inv_dt,
  float max_speed,
  int start,
  int end
) {
  constexpr int simd_width = 8;

  __m256 inv_dt_vec = _mm256_set1_ps(inv_dt);
  __m256 max_speed_vec = _mm256_set1_ps(max_speed);
  __m256 max_speed_sq = _mm256_mul_ps(max_speed_vec, max_speed_vec);

  int i = start;
  for (; i + simd_width <= end; i += simd_width) {
    // Compute vel = (pos - prev) * inv_dt
    __m256 px = _mm256_loadu_ps(&pos.x[i]);
    __m256 py = _mm256_loadu_ps(&pos.y[i]);
    __m256 pz = _mm256_loadu_ps(&pos.z[i]);

    __m256 qx = _mm256_loadu_ps(&prev.x[i]);
    __m256 qy = _mm256_loadu_ps(&prev.y[i]);
    __m256 qz = _mm256_loadu_ps(&prev.z[i]);

    __m256 vx = _mm256_sub_ps(px, qx);
    __m256 vy = _mm256_sub_ps(py, qy);
    __m256 vz = _mm256_sub_ps(pz, qz);

    vx = _mm256_mul_ps(vx, inv_dt_vec);
    vy = _mm256_mul_ps(vy, inv_dt_vec);
    vz = _mm256_mul_ps(vz, inv_dt_vec);

    // Compute length squared
    __m256 len_sq = _mm256_add_ps(
      _mm256_add_ps(_mm256_mul_ps(vx, vx), _mm256_mul_ps(vy, vy)),
      _mm256_mul_ps(vz, vz));

    // Clamp velocities
    __m256 too_fast_mask = _mm256_cmp_ps(len_sq, max_speed_sq, _CMP_GT_OQ);

    // Avoid divide-by-zero: set inv_length to 1 when length == 0
    __m256 length = _mm256_sqrt_ps(len_sq);
    __m256 inv_length = _mm256_blendv_ps(_mm256_rcp_ps(length), _mm256_set1_ps(1.0f), _mm256_cmp_ps(length, _mm256_set1_ps(0.0f), _CMP_EQ_OQ));
    __m256 scale = _mm256_min_ps(_mm256_mul_ps(inv_length, max_speed_vec), _mm256_set1_ps(1.0f));
    scale = _mm256_blendv_ps(_mm256_set1_ps(1.0f), scale, too_fast_mask);

    vx = _mm256_mul_ps(vx, scale);
    vy = _mm256_mul_ps(vy, scale);
    vz = _mm256_mul_ps(vz, scale);

    // Store
    _mm256_storeu_ps(&vel.x[i], vx);
    _mm256_storeu_ps(&vel.y[i], vy);
    _mm256_storeu_ps(&vel.z[i], vz);
  }

  // Scalar fallback
  for (; i < end; ++i) {
    VEC3 v = (pos.get(i) - prev.get(i)) * inv_dt;
    float len = v.length();
    if (len > max_speed)
      v = v.normalized() * max_speed;
    vel.set(i, v);
  }
}


inline float hsum256_ps(__m256 v) {
  __m128 vlow = _mm256_castps256_ps128(v);
  __m128 vhigh = _mm256_extractf128_ps(v, 1);
  __m128 sum1 = _mm_add_ps(vlow, vhigh);
  __m128 shuf = _mm_movehdup_ps(sum1);
  __m128 sum2 = _mm_add_ps(sum1, shuf);
  shuf = _mm_movehl_ps(shuf, sum2);
  __m128 sum3 = _mm_add_ss(sum2, shuf);
  return _mm_cvtss_f32(sum3);
}

inline void apply_displacements_simd(
  float pressure,
  float near_pressure,
  const int* nears_ids,
  const float* nears_closeness,
  const float* nears_dirs_x,
  const float* nears_dirs_y,
  const float* nears_dirs_z,
  int num_nears,
  int idx,
  ParticlesVec* deltas
) {
  const int step = 8;
  int i = 0;

  __m256 p = _mm256_set1_ps(pressure);
  __m256 np = _mm256_set1_ps(near_pressure);
  __m256 half = _mm256_set1_ps(0.5f);

  __m256 accum_dx = _mm256_setzero_ps();
  __m256 accum_dy = _mm256_setzero_ps();
  __m256 accum_dz = _mm256_setzero_ps();

  for (; i + step <= num_nears; i += step) {
    __m256 c = _mm256_loadu_ps(&nears_closeness[i]);
    __m256 amt = _mm256_add_ps(p, _mm256_mul_ps(np, c));
    amt = _mm256_mul_ps(amt, c);
    amt = _mm256_mul_ps(amt, half);

    __m256 vx = _mm256_loadu_ps(&nears_dirs_x[i]);
    __m256 vy = _mm256_loadu_ps(&nears_dirs_y[i]);
    __m256 vz = _mm256_loadu_ps(&nears_dirs_z[i]);

    __m256 dx_final = _mm256_mul_ps(vx, amt);
    __m256 dy_final = _mm256_mul_ps(vy, amt);
    __m256 dz_final = _mm256_mul_ps(vz, amt);

    accum_dx = _mm256_add_ps(accum_dx, dx_final);
    accum_dy = _mm256_add_ps(accum_dy, dy_final);
    accum_dz = _mm256_add_ps(accum_dz, dz_final);

    alignas(32) float tx[8], ty[8], tz[8];
    _mm256_store_ps(tx, dx_final);
    _mm256_store_ps(ty, dy_final);
    _mm256_store_ps(tz, dz_final);

    for (int k = 0; k < 8; ++k)
      deltas->add(nears_ids[i + k], tx[k], ty[k], tz[k]);
  }

  float acc_x = -hsum256_ps(accum_dx);
  float acc_y = -hsum256_ps(accum_dy);
  float acc_z = -hsum256_ps(accum_dz);

  // Scalar fallback
  for (; i < num_nears; ++i) {
    float closeness = nears_closeness[i];
    float amount = (pressure + near_pressure * closeness) * closeness * 0.5f;
    float dx = nears_dirs_x[i] * amount;
    float dy = nears_dirs_y[i] * amount;
    float dz = nears_dirs_z[i] * amount;
    acc_x -= dx;
    acc_y -= dy;
    acc_z -= dz;
    deltas->add(nears_ids[i], dx, dy, dz);
  }

  deltas->add(idx, acc_x, acc_y, acc_z);
}


// for (int i = 0; i < num_particles; ++i)
//   particles_vels.add(i, delta_velocity * masses[particles_type[i]]);
void simd_add_velocity_scaled_by_type(
  ParticlesVec& vels,
  const uint8_t* types,
  const float* masses,         // Assumed size = 4
  const VEC3& delta_velocity,
  int num_particles
) {
  const int step = 8;
  int i = 0;

  // Broadcast delta_velocity components
  __m256 dx = _mm256_set1_ps(delta_velocity.x);
  __m256 dy = _mm256_set1_ps(delta_velocity.y);
  __m256 dz = _mm256_set1_ps(delta_velocity.z);

  // Load masses into 256-bit register (assumes only 4 types)
  alignas(32) float mass_lut[8] = {
    masses[0], masses[1], masses[2], masses[3],
    masses[0], masses[1], masses[2], masses[3]  // repeated for safety
  };

  for (; i + step <= num_particles; i += step) {
    // Load 8 types
    __m128i t8 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&types[i])); // 8 bytes
    __m256i indices = _mm256_cvtepu8_epi32(t8);  // Convert 8x u8 to 8x i32

    // Gather mass values using indices
    __m256 m = _mm256_i32gather_ps(mass_lut, indices, 4);

    // Compute scaled delta
    __m256 vx = _mm256_mul_ps(dx, m);
    __m256 vy = _mm256_mul_ps(dy, m);
    __m256 vz = _mm256_mul_ps(dz, m);

    // Load current velocity
    __m256 v_old_x = _mm256_loadu_ps(&vels.x[i]);
    __m256 v_old_y = _mm256_loadu_ps(&vels.y[i]);
    __m256 v_old_z = _mm256_loadu_ps(&vels.z[i]);

    // Add delta
    v_old_x = _mm256_add_ps(v_old_x, vx);
    v_old_y = _mm256_add_ps(v_old_y, vy);
    v_old_z = _mm256_add_ps(v_old_z, vz);

    // Store back
    _mm256_storeu_ps(&vels.x[i], v_old_x);
    _mm256_storeu_ps(&vels.y[i], v_old_y);
    _mm256_storeu_ps(&vels.z[i], v_old_z);
  }

  // Scalar fallback for tail
  for (; i < num_particles; ++i) {
    float m = masses[types[i]];
    vels.x[i] += delta_velocity.x * m;
    vels.y[i] += delta_velocity.y * m;
    vels.z[i] += delta_velocity.z * m;
  }
}


inline void collect_neighbors_block(
  const ParticlesVec& pos,
  float kernel_radius,
  float kernel_radius_inv,
  float* density_acc,
  float* near_density_acc,
  int*   nears_ids,
  float* nears_closeness,
  float* nears_dirs_x,
  float* nears_dirs_y,
  float* nears_dirs_z,
  int& num_nears,
  int max_nears,
  int i,            // current particle i
  int j_start,      // start of neighbor block
  int mask_range
) {
  __m256 pi_x = _mm256_set1_ps(pos.x[i]);
  __m256 pi_y = _mm256_set1_ps(pos.y[i]);
  __m256 pi_z = _mm256_set1_ps(pos.z[i]);

  __m256 px = _mm256_loadu_ps(&pos.x[j_start]);
  __m256 py = _mm256_loadu_ps(&pos.y[j_start]);
  __m256 pz = _mm256_loadu_ps(&pos.z[j_start]);

  __m256 dx = _mm256_sub_ps(px, pi_x);
  __m256 dy = _mm256_sub_ps(py, pi_y);
  __m256 dz = _mm256_sub_ps(pz, pi_z);

  __m256 d2 = _mm256_add_ps(
    _mm256_add_ps(_mm256_mul_ps(dx, dx), _mm256_mul_ps(dy, dy)),
    _mm256_mul_ps(dz, dz)
  );

  __m256 length = _mm256_sqrt_ps(d2);

  __m256 mask_valid = _mm256_and_ps(
    _mm256_cmp_ps(length, _mm256_set1_ps(kernel_radius), _CMP_LT_OQ),
    _mm256_cmp_ps(length, _mm256_set1_ps(1e-3f), _CMP_GT_OQ)
  );

  // Exclude self-particle
  __m256i indices = _mm256_add_epi32(_mm256_set1_epi32(j_start), _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
  __m256i i_vec = _mm256_set1_epi32(i);
  __m256 mask_self = _mm256_castsi256_ps(_mm256_cmpeq_epi32(indices, i_vec));

  __m256 mask = _mm256_andnot_ps(mask_self, mask_valid);
  int mask_bits = _mm256_movemask_ps(mask) & mask_range;

  // Load inverse r and normalize
  __m256 r = _mm256_add_ps(length, _mm256_set1_ps(1e-5f));
  __m256 inv_r = _mm256_div_ps(_mm256_set1_ps(1.0f), r);
  dx = _mm256_mul_ps(dx, inv_r);
  dy = _mm256_mul_ps(dy, inv_r);
  dz = _mm256_mul_ps(dz, inv_r);

  // Closeness
  __m256 q = _mm256_mul_ps(r, _mm256_set1_ps(kernel_radius_inv));
  __m256 closeness = _mm256_sub_ps(_mm256_set1_ps(1.0f), q);

  const float* c_ptr = reinterpret_cast<const float*>(&closeness);
  const float* dx_ptr = reinterpret_cast<const float*>(&dx);
  const float* dy_ptr = reinterpret_cast<const float*>(&dy);
  const float* dz_ptr = reinterpret_cast<const float*>(&dz);

  // Iterate over the 8 lanes
  // Skip if the mask is 0, means does not apply to this range or is too far
  int lane = 0;
  while (mask_bits) {
    if (mask_bits & 1) {
      float c = c_ptr[lane];
      float c_sq = c * c;
      float c_cu = c_sq * c;

      *density_acc += c_sq;
      *near_density_acc += c_cu;

      int j = j_start + lane;
      nears_ids[num_nears] = j;
      nears_closeness[num_nears] = c;
      nears_dirs_x[num_nears] = dx_ptr[lane];
      nears_dirs_y[num_nears] = dy_ptr[lane];
      nears_dirs_z[num_nears] = dz_ptr[lane];
      ++num_nears;
      if (num_nears >= max_nears)
        break;
    }

    ++lane;
    mask_bits >>= 1;
  }
}

void ViscoelasticSim::init() {
  setNumThreads(num_threads);

  assigned_cells.resize(max_particles);

  num_particles = 0;
  particles_pos.resize(max_particles);
  particles_prev_pos.resize(max_particles);
  particles_vels.resize(max_particles);
  particles_frozen_pos.resize(max_particles);
  particles_type = new u8[max_particles];

  aux_particles_pos.resize(max_particles);
  aux_particles_prev_pos.resize(max_particles);
  aux_particles_vels.resize(max_particles);
  aux_particles_type = new u8[max_particles];
}

void ViscoelasticSim::addParticle(VEC3 pos, VEC3 vel, uint8_t particle_type) {
  if (num_particles >= max_particles)
    return;
  assert(particles_pos.buf.size() > 0);
  particles_pos.set(num_particles, pos);
  particles_prev_pos.set(num_particles, pos);
  particles_vels.set(num_particles, vel);
  particles_type[num_particles] = particle_type;
  ++num_particles;
}

void ViscoelasticSim::resolveCollisions(float dt, int start, int end) {
  PROFILE_SCOPED_NAMED("resolveCollisions");
  float inv_world_scale = 1.0f / world_scale;
  const float boundaryMul = -0.5f * dt * dt * world_scale;
  for (int i = start; i < end; ++i) {
    VEC3 p = particles_pos.get(i);
    VEC3 pq = (p)*inv_world_scale;
    float d = sdf.evalCompact(pq);
    if (d < 0.0f) {
      VEC3 grad = sdf.evalGradCompact(pq);
      particles_pos.add(i, grad * d * boundaryMul);
    }
  }
}

void ViscoelasticSim::processRange(float dt, const CPUSpatialSubdivision::CellRange& range, const ParticlesVec& __restrict ppos, ParticlesVec* __restrict deltas) {
  float kernel_radius = mat.kernel_radius;
  float kernel_radius_inv = 1.0f / kernel_radius;
  float rest_density = mat.rest_density;
  float stiffness = mat.stiffness * dt * dt;
  float near_stiffness = mat.near_stiffness * dt * dt;

  constexpr static int max_nears = 64;
  int nears_ids[max_nears];
  float nears_closeness[max_nears];

  alignas(32) float nears_dirs_x[max_nears];
  alignas(32) float nears_dirs_y[max_nears];
  alignas(32) float nears_dirs_z[max_nears];

  //PROFILE_SCOPED_NAMED("CR");
  spatial_hash.onEachParticleInCell(range, [&](int i, const CPUSpatialSubdivision::NearRanges& near_ranges) {
    float density = 0.0f;
    float near_density = 0.0f;
    int num_nears = 0;

    // Iterate over all 27 non-empty surrounding cells
    using u32 = uint32_t;
    for (u32 r = 0; r < near_ranges.n && num_nears < max_nears; ++r) {

      // All the particles in a cell are stored in continuous range
      u32 first = near_ranges.ranges[r].first;
      u32 last = near_ranges.ranges[r].last;
      for (u32 j = first; j < last && num_nears < max_nears; j += 8) {

        int count = last - j;
        // We process particles in blocks of 8.
        // If the block is smaller than 8, pass a mask to discard those particles slots
        int mask_range = (1 << std::min(8, count)) - 1;
        collect_neighbors_block(
          ppos,
          kernel_radius, kernel_radius_inv,
          &density, &near_density,
          nears_ids, nears_closeness, nears_dirs_x, nears_dirs_y, nears_dirs_z,
          num_nears, max_nears,
          i, j, mask_range
        );
      }
    }

    near_density = std::max(0.0f, near_density);
    float pressure = stiffness * (density - rest_density);
    float near_pressure = near_stiffness * near_density;

    pressure = std::min(1.0f, pressure);
    near_pressure = std::min(1.0f, near_pressure);

    apply_displacements_simd(
      pressure, near_pressure,
      nears_ids, nears_closeness,
      nears_dirs_x, nears_dirs_y, nears_dirs_z,
      num_nears,
      i,
      deltas
    );

    });
}


void ViscoelasticSim::updateSpatialHash() {

  float inv_kernel_radius = 1.0f / mat.kernel_radius;
  spatial_hash.setGridScale(inv_kernel_radius);

  // We are going to reorder the particles, by moving the
  // from the old order to the new order
  particles_pos.swap(aux_particles_pos);
  particles_vels.swap(aux_particles_vels);
  particles_prev_pos.swap(aux_particles_prev_pos);
  std::swap(particles_type, aux_particles_type);
  {
    // Precompute for each particle it's icoords and cell_id
    PROFILE_SCOPED_NAMED("assignedCells");
    runInParallel(num_particles, num_threads, [&](int start, int end, int job_id) {
      for (int i = start; i < end; ++i) {
        VEC3 pos = aux_particles_pos.get(i);
        CPUSpatialSubdivision::Int3 ipos = spatial_hash.gridCoords(pos);
        uint32_t cell_id = spatial_hash.gridHash(ipos);
        assigned_cells[i] = { ipos, cell_id };
      }
      });
  }

  spatial_hash.setPoints(assigned_cells.data(), num_particles);

  runInParallel(num_particles, num_threads, [&](int start, int end, int job_id) {
    PROFILE_SCOPED_NAMED("sortParticles");
    bool debug_particle_changed = false;
    spatial_hash.sortParticles(start, end, [&](int j, int i) {
      if (!debug_particle_changed && j == debug_particle) {
        debug_particle_changed = true;
        debug_particle = i;
      }
      assert(i >= 0 && i < max_particles);
      assert(j >= 0 && j < max_particles);
      particles_pos.set(i, aux_particles_pos.get(j));
      particles_vels.set(i, aux_particles_vels.get(j));
      particles_prev_pos.set(i, aux_particles_prev_pos.get(j));
      particles_type[i] = aux_particles_type[j];
      });
    });

}

void ViscoelasticSim::doubleDensityRelaxationPara(float dt, ThreadPool& pool) {
  int num_jobs = (int)spatial_hash.cells_ranges.size();
  runInParallel(num_jobs, num_threads * 3, [&](int start, int end, int job_id) {
    for (int i = start; i < end; ++i)
      processRange(dt, spatial_hash.cells_ranges[i], particles_frozen_pos, &particles_pos);
    });
}

void ViscoelasticSim::doubleDensityRelaxation(float dt) {
  for (auto& range : spatial_hash.cells_ranges)
    processRange(dt, range, particles_frozen_pos, &particles_pos);
}

void ViscoelasticSim::removeParticle(int id) {
  auto swapInContainer = [](ParticlesVec& container, int a, int b) {
    VEC3 pa = container.get(a);
    VEC3 pb = container.get(b);
    container.set(b, pa);
    container.set(a, pb);
    };

  swapInContainer(particles_pos, num_particles - 1, id);
  swapInContainer(particles_vels, num_particles - 1, id);
  std::swap(particles_type[num_particles - 1], particles_type[id]);
  num_particles -= 1;
}

void ViscoelasticSim::removeParticles(std::vector<int>& particles_to_remove) {
  std::sort(particles_to_remove.begin(), particles_to_remove.end(), [](int a, int b) { return a > b; });
  for (auto id : particles_to_remove)
    removeParticle(id);
  particles_to_remove.clear();
}

void ViscoelasticSim::getParticleIDsNear(std::vector<int>& out_ids, VEC3 ref_point, float rad) const {
  float interact_rad_sqr = rad * rad;
  for (int i = 0; i < num_particles; ++i) {
    VEC3 delta = particles_pos.get(i) - ref_point;
    float dist_sq = delta.lengthSquared();
    if (dist_sq > interact_rad_sqr || dist_sq < 0.1f)
      continue;
    out_ids.push_back(i);
  }
}

void ViscoelasticSim::updateStep(float dt) {

  {
    TTimer tm;
    updateSpatialHash();
    saveTime(eSection::SpatialHash, tm);
  }

  // Apply external forces
  {
    TTimer tm;
    PROFILE_SCOPED_NAMED("velocities");
    VEC3 delta_velocity = 0.02f * mat.kernel_radius * mat.gravity * dt;
    simd_add_velocity_scaled_by_type(particles_vels, particles_type, masses, delta_velocity, num_particles);
    saveTime(eSection::VelocitiesUpdate, tm);
  }

  {
    PROFILE_SCOPED_NAMED("ext forces");
    float attrack_repel = attract ? 0.01f * mat.kernel_radius : 0.0f;
    attrack_repel -= repel ? 0.01f * mat.kernel_radius : 0.0f;
    bool attrack_repel_active = attrack_repel != 0.0f;
    if (attrack_repel_active) {
      float interact_rad_sqr = interact_rad * interact_rad;

      for (int i = 0; i < num_particles; ++i) {
        VEC3 delta = particles_pos.get(i) - interact_point;
        float dist_sq = delta.lengthSquared();
        if (dist_sq > interact_rad_sqr || dist_sq < 0.1f)
          continue;
        const float dist = sqrtf(dist_sq);
        const float inv_dist = 1.0f / dist;
        delta *= inv_dist;
        particles_vels.add(i, attrack_repel * (-delta));
      }
    }
  }

  {
    TTimer tm;
    PROFILE_SCOPED_NAMED("predict position");
    particles_prev_pos.copyFrom(particles_pos, num_particles);
    simd_update_positions(particles_pos, particles_vels, dt, num_particles);
    saveTime(eSection::PredictPositions, tm);
  }

  {
    PROFILE_SCOPED_NAMED("freeze_positions");
    particles_frozen_pos.copyFrom(particles_pos, num_particles);
  }

  if (in_2d) {
    for (int i = 0; i < num_particles; ++i) {
      particles_pos.x[i] = 0.01f;
      particles_vels.x[i] = 0.0f;
    }
  }

  {
    TTimer tm;
    PROFILE_SCOPED_NAMED("relaxation");
    if (using_parallel)
      doubleDensityRelaxationPara(dt, *pool);
    else
      doubleDensityRelaxation(dt);
    saveTime(eSection::Relaxation, tm);
  }

  {
    TTimer tm;
    PROFILE_SCOPED_NAMED("collisions");
    runInParallel(num_particles, num_threads * 3, [&](int start, int end, int job_id) {
      resolveCollisions(dt, start, end);
      });
    saveTime(eSection::Collisions, tm);
  }

  {
    TTimer tm;
    PROFILE_SCOPED_NAMED("velocities_from_positions");
    float inv_dt = 1.0f / dt;
    runInParallel(num_particles, 4, [&](int start, int end, int job_id) {
      simd_update_velocities_clamped(particles_vels, particles_pos, particles_prev_pos, inv_dt, max_speed, start, end);
      });
    saveTime(eSection::VelocitiesFromPositions, tm);
  }

}

void ViscoelasticSim::setNumThreads(int new_num_threads) {
  num_threads = new_num_threads;
  if (pool)
    delete pool;
  pool = new ThreadPool(num_threads);
}

void ViscoelasticSim::update(float delta_time) {
  sdf.generateCompactStructs();
  float dt = delta_time / (float)num_substeps;
  TTimer tm;
  for (int i = 0; i < num_substeps; ++i)
    updateStep(dt);
  saveTime(eSection::Update, tm);
}