#include "platform.h"
#include "viscoelastic_sim.h"
#include <immintrin.h>
#include <array>

namespace {
constexpr int kHierarchyMaxMacroSide = 8;
constexpr int kHierarchyMaxLocalCellCount =
  kHierarchyMaxMacroSide * kHierarchyMaxMacroSide * kHierarchyMaxMacroSide;

void sortAndCoalesceNearRanges(CPUSpatialSubdivision::NearRanges& near_ranges) {
  if (near_ranges.n < 2)
    return;

  std::sort(
    near_ranges.ranges,
    near_ranges.ranges + near_ranges.n,
    [](const auto& a, const auto& b) { return a.first < b.first; });

  uint32_t output_count = 1;
  for (uint32_t i = 1; i < near_ranges.n; ++i) {
    auto& previous = near_ranges.ranges[output_count - 1];
    const auto current = near_ranges.ranges[i];
    if (current.first <= previous.last)
      previous.last = std::max(previous.last, current.last);
    else
      near_ranges.ranges[output_count++] = current;
  }
  near_ranges.n = output_count;
}
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
  ParticlesVec* out_deltas
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
      out_deltas->add(nears_ids[i + k], tx[k], ty[k], tz[k]);
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
    out_deltas->add(nears_ids[i], dx, dy, dz);
  }

  out_deltas->add(idx, acc_x, acc_y, acc_z);
}

// A worker-private touched map at 64-particle granularity was tested here.
// It skipped roughly 55% of worker/range reads, but tracking and selecting the
// ranges produced no measurable end-to-end gain at 32K or 64K particles. Keep
// the dense streaming reduction: it is simpler and performs at least as well.
void simd_apply_relaxation_deltas(
  ParticlesVec& positions,
  std::vector<ParticlesVec>& worker_deltas,
  int start,
  int end
) {
  constexpr int step = 8;
  int i = start;
  const __m256 zero = _mm256_setzero_ps();

  for (; i + step <= end; i += step) {
    __m256 px = _mm256_loadu_ps(&positions.x[i]);
    __m256 py = _mm256_loadu_ps(&positions.y[i]);
    __m256 pz = _mm256_loadu_ps(&positions.z[i]);

    for (ParticlesVec& deltas : worker_deltas) {
      const __m256 dx = _mm256_loadu_ps(&deltas.x[i]);
      const __m256 dy = _mm256_loadu_ps(&deltas.y[i]);
      const __m256 dz = _mm256_loadu_ps(&deltas.z[i]);
      px = _mm256_add_ps(px, dx);
      py = _mm256_add_ps(py, dy);
      pz = _mm256_add_ps(pz, dz);

      _mm256_storeu_ps(&deltas.x[i], zero);
      _mm256_storeu_ps(&deltas.y[i], zero);
      _mm256_storeu_ps(&deltas.z[i], zero);
    }

    _mm256_storeu_ps(&positions.x[i], px);
    _mm256_storeu_ps(&positions.y[i], py);
    _mm256_storeu_ps(&positions.z[i], pz);
  }

  for (; i < end; ++i) {
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    for (ParticlesVec& deltas : worker_deltas) {
      dx += deltas.x[i];
      dy += deltas.y[i];
      dz += deltas.z[i];
      deltas.x[i] = 0.0f;
      deltas.y[i] = 0.0f;
      deltas.z[i] = 0.0f;
    }
    positions.add(i, dx, dy, dz);
  }
}

void simd_prepare_particles(
  ParticlesVec& pos,
  ParticlesVec& prev,
  ParticlesVec& frozen,
  ParticlesVec& vels,
  const uint8_t* types,
  const float* masses,
  const VEC3& delta_velocity,
  float dt,
  bool in_2d,
  float attract_repel,
  const VEC3& interact_point,
  float interact_rad,
  int start,
  int end
) {
  // Attraction/repulsion is an interactive, normally inactive path. Keep its
  // exact per-particle ordering while still allowing independent chunks to run
  // in parallel.
  if (attract_repel != 0.0f) {
    const float interact_rad_sq = interact_rad * interact_rad;
    for (int i = start; i < end; ++i) {
      const VEC3 old_pos = pos.get(i);
      VEC3 velocity = vels.get(i) + delta_velocity * masses[types[i]];
      VEC3 delta = old_pos - interact_point;
      const float distance_sq = delta.lengthSquared();
      if (distance_sq <= interact_rad_sq && distance_sq >= 0.1f)
        velocity += attract_repel * (-delta * (1.0f / sqrtf(distance_sq)));

      const VEC3 predicted = old_pos + velocity * dt;
      prev.set(i, old_pos);
      frozen.set(i, predicted);
      if (in_2d) {
        pos.set(i, VEC3(0.01f, predicted.y, predicted.z));
        velocity.x = 0.0f;
      }
      else {
        pos.set(i, predicted);
      }
      vels.set(i, velocity);
    }
    return;
  }

  constexpr int simd_width = 8;
  const __m256 dt_vec = _mm256_set1_ps(dt);
  const __m256 dv_x = _mm256_set1_ps(delta_velocity.x);
  const __m256 dv_y = _mm256_set1_ps(delta_velocity.y);
  const __m256 dv_z = _mm256_set1_ps(delta_velocity.z);
  const __m256 zero = _mm256_setzero_ps();
  const __m256 plane_x = _mm256_set1_ps(0.01f);
  alignas(32) const float mass_lut[8] = {
    masses[0], masses[1], masses[2], masses[3],
    masses[0], masses[1], masses[2], masses[3]
  };

  int i = start;
  for (; i + simd_width <= end; i += simd_width) {
    const __m128i types8 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&types[i]));
    const __m256i mass_indices = _mm256_cvtepu8_epi32(types8);
    const __m256 mass = _mm256_i32gather_ps(mass_lut, mass_indices, 4);

    const __m256 old_x = _mm256_loadu_ps(&pos.x[i]);
    const __m256 old_y = _mm256_loadu_ps(&pos.y[i]);
    const __m256 old_z = _mm256_loadu_ps(&pos.z[i]);
    __m256 vel_x = _mm256_loadu_ps(&vels.x[i]);
    __m256 vel_y = _mm256_loadu_ps(&vels.y[i]);
    __m256 vel_z = _mm256_loadu_ps(&vels.z[i]);

    vel_x = _mm256_add_ps(vel_x, _mm256_mul_ps(dv_x, mass));
    vel_y = _mm256_add_ps(vel_y, _mm256_mul_ps(dv_y, mass));
    vel_z = _mm256_add_ps(vel_z, _mm256_mul_ps(dv_z, mass));

    const __m256 predicted_x = _mm256_add_ps(old_x, _mm256_mul_ps(vel_x, dt_vec));
    const __m256 predicted_y = _mm256_add_ps(old_y, _mm256_mul_ps(vel_y, dt_vec));
    const __m256 predicted_z = _mm256_add_ps(old_z, _mm256_mul_ps(vel_z, dt_vec));

    _mm256_storeu_ps(&prev.x[i], old_x);
    _mm256_storeu_ps(&prev.y[i], old_y);
    _mm256_storeu_ps(&prev.z[i], old_z);
    _mm256_storeu_ps(&frozen.x[i], predicted_x);
    _mm256_storeu_ps(&frozen.y[i], predicted_y);
    _mm256_storeu_ps(&frozen.z[i], predicted_z);
    _mm256_storeu_ps(&pos.x[i], in_2d ? plane_x : predicted_x);
    _mm256_storeu_ps(&pos.y[i], predicted_y);
    _mm256_storeu_ps(&pos.z[i], predicted_z);
    _mm256_storeu_ps(&vels.x[i], in_2d ? zero : vel_x);
    _mm256_storeu_ps(&vels.y[i], vel_y);
    _mm256_storeu_ps(&vels.z[i], vel_z);
  }

  for (; i < end; ++i) {
    const VEC3 old_pos = pos.get(i);
    VEC3 velocity = vels.get(i) + delta_velocity * masses[types[i]];
    const VEC3 predicted = old_pos + velocity * dt;
    prev.set(i, old_pos);
    frozen.set(i, predicted);
    if (in_2d) {
      pos.set(i, VEC3(0.01f, predicted.y, predicted.z));
      velocity.x = 0.0f;
    }
    else {
      pos.set(i, predicted);
    }
    vels.set(i, velocity);
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
  int lane_count
) {
  __m256 pi_x = _mm256_set1_ps(pos.x[i]);
  __m256 pi_y = _mm256_set1_ps(pos.y[i]);
  __m256 pi_z = _mm256_set1_ps(pos.z[i]);

  __m256 px;
  __m256 py;
  __m256 pz;
  int mask_range;
  if (lane_count == 8) {
    px = _mm256_loadu_ps(&pos.x[j_start]);
    py = _mm256_loadu_ps(&pos.y[j_start]);
    pz = _mm256_loadu_ps(&pos.z[j_start]);
    mask_range = 0xff;
  }
  else {
    // A normal unaligned load can run past the z allocation when the particle
    // capacity is full. Masked loads make the final partial block safe.
    const __m256i lane_ids = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    const __m256i load_mask = _mm256_cmpgt_epi32(_mm256_set1_epi32(lane_count), lane_ids);
    px = _mm256_maskload_ps(&pos.x[j_start], load_mask);
    py = _mm256_maskload_ps(&pos.y[j_start], load_mask);
    pz = _mm256_maskload_ps(&pos.z[j_start], load_mask);
    mask_range = (1 << lane_count) - 1;
  }

  __m256 dx = _mm256_sub_ps(px, pi_x);
  __m256 dy = _mm256_sub_ps(py, pi_y);
  __m256 dz = _mm256_sub_ps(pz, pi_z);

  __m256 d2 = _mm256_add_ps(
    _mm256_add_ps(_mm256_mul_ps(dx, dx), _mm256_mul_ps(dy, dy)),
    _mm256_mul_ps(dz, dz)
  );

  const __m256 radius_sq = _mm256_set1_ps(kernel_radius * kernel_radius);
  const __m256 min_dist_sq = _mm256_set1_ps(1e-6f);
  __m256 mask_valid = _mm256_and_ps(
    _mm256_cmp_ps(d2, radius_sq, _CMP_LT_OQ),
    _mm256_cmp_ps(d2, min_dist_sq, _CMP_GT_OQ)
  );

  // Exclude self-particle
  __m256i indices = _mm256_add_epi32(_mm256_set1_epi32(j_start), _mm256_set_epi32(7, 6, 5, 4, 3, 2, 1, 0));
  __m256i i_vec = _mm256_set1_epi32(i);
  __m256 mask_self = _mm256_castsi256_ps(_mm256_cmpeq_epi32(indices, i_vec));

  __m256 mask = _mm256_andnot_ps(mask_self, mask_valid);
  int mask_bits = _mm256_movemask_ps(mask) & mask_range;
  if (mask_bits == 0)
    return;

  // Keep the original precise normalization. The squared-distance test above
  // still lets us skip the square root and division for wholly rejected blocks.
  const __m256 one = _mm256_set1_ps(1.0f);
  const __m256 length = _mm256_sqrt_ps(d2);
  const __m256 r = _mm256_add_ps(length, _mm256_set1_ps(1e-5f));
  const __m256 inv_r = _mm256_div_ps(one, r);

  dx = _mm256_mul_ps(dx, inv_r);
  dy = _mm256_mul_ps(dy, inv_r);
  dz = _mm256_mul_ps(dz, inv_r);

  // Closeness
  __m256 q = _mm256_mul_ps(r, _mm256_set1_ps(kernel_radius_inv));
  __m256 closeness = _mm256_sub_ps(one, q);

  alignas(32) float c_values[8];
  alignas(32) float dx_values[8];
  alignas(32) float dy_values[8];
  alignas(32) float dz_values[8];
  _mm256_store_ps(c_values, closeness);
  _mm256_store_ps(dx_values, dx);
  _mm256_store_ps(dy_values, dy);
  _mm256_store_ps(dz_values, dz);

  // Iterate over the 8 lanes
  // Skip if the mask is 0, means does not apply to this range or is too far
  int lane = 0;
  while (mask_bits) {
    if (mask_bits & 1) {
      float c = c_values[lane];
      float c_sq = c * c;
      float c_cu = c_sq * c;

      *density_acc += c_sq;
      *near_density_acc += c_cu;

      nears_ids[num_nears] = j_start + lane;
      nears_closeness[num_nears] = c;
      nears_dirs_x[num_nears] = dx_values[lane];
      nears_dirs_y[num_nears] = dy_values[lane];
      nears_dirs_z[num_nears] = dz_values[lane];
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
  for (ParticlesVec& deltas : relaxation_worker_deltas)
    deltas.set(num_particles, VEC3::zero);
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

void ViscoelasticSim::processRange(float dt, const CPUSpatialSubdivision::CellRange& range, const CPUSpatialSubdivision::NearRanges& near_ranges, const ParticlesVec& __restrict ppos, ParticlesVec* __restrict out_deltas) {
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
  for (uint32_t i = range.range.first; i < range.range.last; ++i) {
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
        // If the block is smaller than 8, use masked loads for those slots.
        int lane_count = std::min(8, count);
        collect_neighbors_block(
          ppos,
          kernel_radius, kernel_radius_inv,
          &density, &near_density,
          nears_ids, nears_closeness, nears_dirs_x, nears_dirs_y, nears_dirs_z,
          num_nears, max_nears,
          i, j, lane_count
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
      out_deltas
    );

  }
}


void ViscoelasticSim::assignCellsParallel() {
  PROFILE_SCOPED_NAMED("assignCellsParallel");

  // Buckets must be a power of two because we select them from the low hash
  // bits. Normalize the UI value here so arbitrary intermediate values remain
  // safe while dragging it.
  int num_buckets = 8;
  const int requested_buckets = std::max(8, std::min(spatial_index_buckets, 128));
  while (num_buckets < requested_buckets)
    num_buckets <<= 1;
  spatial_index_buckets = num_buckets;

  const int num_partitions = std::max(1, std::min(num_threads, num_particles));
  const uint32_t bucket_mask = (uint32_t)num_buckets - 1;
  const size_t partition_bucket_count = (size_t)num_partitions * num_buckets;

  spatial_bucket_counts.assign(partition_bucket_count, 0);
  spatial_bucket_offsets.resize(partition_bucket_count);
  spatial_bucket_starts.resize((size_t)num_buckets + 1);
  spatial_bucket_particle_ids.resize(num_particles);
  spatial_bucket_unique_counts.resize(num_buckets);
  spatial_bucket_unique_offsets.resize((size_t)num_buckets + 1);

  {
    PROFILE_SCOPED_NAMED("parallelCellHistogram");
    runInParallel(num_particles, num_partitions, [&](int start, int end, int job_id) {
      uint32_t* counts = spatial_bucket_counts.data() + (size_t)job_id * num_buckets;
      for (int particle_id = start; particle_id < end; ++particle_id) {
        const uint32_t bucket = assigned_cells[particle_id].cell_id & bucket_mask;
        ++counts[bucket];
      }
      });
  }

  {
    PROFILE_SCOPED_NAMED("parallelCellPrefixScan");
    uint32_t particle_offset = 0;
    for (int bucket = 0; bucket < num_buckets; ++bucket) {
      spatial_bucket_starts[bucket] = particle_offset;
      for (int partition = 0; partition < num_partitions; ++partition) {
        const size_t idx = (size_t)partition * num_buckets + bucket;
        spatial_bucket_offsets[idx] = particle_offset;
        particle_offset += spatial_bucket_counts[idx];
      }
    }
    spatial_bucket_starts[num_buckets] = particle_offset;
    assert(particle_offset == (uint32_t)num_particles);
  }

  {
    PROFILE_SCOPED_NAMED("parallelCellScatter");
    runInParallel(num_particles, num_partitions, [&](int start, int end, int job_id) {
      uint32_t* offsets = spatial_bucket_offsets.data() + (size_t)job_id * num_buckets;
      for (int particle_id = start; particle_id < end; ++particle_id) {
        const uint32_t bucket = assigned_cells[particle_id].cell_id & bucket_mask;
        spatial_bucket_particle_ids[offsets[bucket]++] = (uint32_t)particle_id;
      }
      });
  }

  // Give every bucket a private, open-addressed table at <= 50% load. The
  // tables occupy contiguous scratch storage but are initialized and mutated
  // only by their owning bucket job.
  spatial_bucket_hash_offsets.resize((size_t)num_buckets + 1);
  uint32_t total_hash_capacity = 0;
  for (int bucket = 0; bucket < num_buckets; ++bucket) {
    spatial_bucket_hash_offsets[bucket] = total_hash_capacity;
    const uint32_t particle_count =
      spatial_bucket_starts[bucket + 1] - spatial_bucket_starts[bucket];
    uint32_t capacity = 0;
    if (particle_count > 0) {
      capacity = 2;
      while (capacity < particle_count * 2)
        capacity <<= 1;
    }
    total_hash_capacity += capacity;
  }
  spatial_bucket_hash_offsets[num_buckets] = total_hash_capacity;
  spatial_local_hash_coords.resize(total_hash_capacity);
  spatial_local_hash_unique_indices.resize(total_hash_capacity);
  spatial_particle_unique_indices.resize(num_particles);
  spatial_particle_indices_in_cell.resize(num_particles);
  spatial_provisional_unique_cells.resize(num_particles);

  auto secondary_coord_hash = [](const CPUSpatialSubdivision::Int3& coords) {
    uint32_t hash = static_cast<uint32_t>(coords.x) * 0x8da6b343u;
    hash ^= static_cast<uint32_t>(coords.y) * 0xd8163841u;
    hash ^= static_cast<uint32_t>(coords.z) * 0xcb1ab31fu;
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    return hash;
  };

  {
    PROFILE_SCOPED_NAMED("parallelCellGroup");
    runInParallel(num_buckets, num_buckets, [&](int start, int end, int job_id) {
      for (int bucket = start; bucket < end; ++bucket) {
        const uint32_t hash_first = spatial_bucket_hash_offsets[bucket];
        const uint32_t hash_last = spatial_bucket_hash_offsets[bucket + 1];
        if (hash_first == hash_last) {
          spatial_bucket_unique_counts[bucket] = 0;
          continue;
        }

        std::fill(
          spatial_local_hash_unique_indices.begin() + hash_first,
          spatial_local_hash_unique_indices.begin() + hash_last,
          UINT32_MAX);

        const uint32_t hash_mask = hash_last - hash_first - 1;
        const uint32_t particle_first = spatial_bucket_starts[bucket];
        const uint32_t particle_last = spatial_bucket_starts[bucket + 1];
        uint32_t unique_count = 0;

        for (uint32_t i = particle_first; i < particle_last; ++i) {
          const uint32_t particle_id = spatial_bucket_particle_ids[i];
          const auto coords = assigned_cells[particle_id].ipos;
          uint32_t hash_slot = hash_first + (secondary_coord_hash(coords) & hash_mask);

          while (true) {
            uint32_t& local_unique_idx = spatial_local_hash_unique_indices[hash_slot];
            if (local_unique_idx == UINT32_MAX) {
              local_unique_idx = unique_count++;
              spatial_local_hash_coords[hash_slot] = coords;
              spatial_provisional_unique_cells[particle_first + local_unique_idx] = {
                coords,
                assigned_cells[particle_id].cell_id,
                1,
                0
              };
              spatial_particle_unique_indices[particle_id] = local_unique_idx;
              spatial_particle_indices_in_cell[particle_id] = 0;
              break;
            }

            if (spatial_local_hash_coords[hash_slot] == coords) {
              auto& unique_cell =
                spatial_provisional_unique_cells[particle_first + local_unique_idx];
              spatial_particle_unique_indices[particle_id] = local_unique_idx;
              spatial_particle_indices_in_cell[particle_id] = unique_cell.num_particles++;
              break;
            }

            hash_slot = hash_first + ((hash_slot - hash_first + 1) & hash_mask);
          }
        }
        spatial_bucket_unique_counts[bucket] = unique_count;
      }
      });
  }

  uint32_t num_unique_cells = 0;
  for (int bucket = 0; bucket < num_buckets; ++bucket) {
    spatial_bucket_unique_offsets[bucket] = num_unique_cells;
    num_unique_cells += spatial_bucket_unique_counts[bucket];
  }
  spatial_bucket_unique_offsets[num_buckets] = num_unique_cells;
  spatial_unique_cells.resize(num_unique_cells);
  spatial_source_unique_indices.resize(num_unique_cells);

  {
    PROFILE_SCOPED_NAMED("parallelOrderUniqueCells");
    spatial_partition_unique_offsets.resize((size_t)num_partitions + 1);

    // Mark/count cell heads per contiguous particle partition. The particles
    // already carry idx_in_cell from parallelCellGroup, so no flag buffer is
    // needed.
    runInParallel(num_particles, num_partitions, [&](int start, int end, int job_id) {
      uint32_t count = 0;
      for (int particle_id = start; particle_id < end; ++particle_id)
        count += spatial_particle_indices_in_cell[particle_id] == 0;
      spatial_partition_unique_offsets[job_id] = count;
      });

    uint32_t unique_offset = 0;
    for (int partition = 0; partition < num_partitions; ++partition) {
      const uint32_t count = spatial_partition_unique_offsets[partition];
      spatial_partition_unique_offsets[partition] = unique_offset;
      unique_offset += count;
    }
    spatial_partition_unique_offsets[num_partitions] = unique_offset;
    assert(unique_offset == num_unique_cells);

    // Scatter the unique records into first-particle order. Concatenating the
    // contiguous partitions preserves the same spatial coherence as the old
    // serial orderUniqueCells pass.
    runInParallel(num_particles, num_partitions, [&](int start, int end, int job_id) {
      uint32_t destination = spatial_partition_unique_offsets[job_id];
      for (int particle_id = start; particle_id < end; ++particle_id) {
        if (spatial_particle_indices_in_cell[particle_id] != 0)
          continue;

        const uint32_t bucket = assigned_cells[particle_id].cell_id & bucket_mask;
        const uint32_t local_unique_idx = spatial_particle_unique_indices[particle_id];
        const uint32_t source_unique_idx =
          spatial_bucket_unique_offsets[bucket] + local_unique_idx;
        const uint32_t provisional_idx =
          spatial_bucket_starts[bucket] + local_unique_idx;
        auto unique_cell = spatial_provisional_unique_cells[provisional_idx];
        unique_cell.source_idx = source_unique_idx;
        spatial_unique_cells[destination++] = unique_cell;
      }
      assert(destination == spatial_partition_unique_offsets[job_id + 1]);
      });
  }

  {
    PROFILE_SCOPED_NAMED("sortUniqueCells");
    const auto spatial_less = [](const auto& a, const auto& b) {
      if (a.ipos.y != b.ipos.y)
        return a.ipos.y < b.ipos.y;
      if (a.ipos.x != b.ipos.x)
        return a.ipos.x < b.ipos.x;
      return a.ipos.z < b.ipos.z;
    };
    if (!std::is_sorted(spatial_unique_cells.begin(), spatial_unique_cells.end(), spatial_less))
      std::sort(spatial_unique_cells.begin(), spatial_unique_cells.end(), spatial_less);
    for (uint32_t unique_idx = 0; unique_idx < num_unique_cells; ++unique_idx)
      spatial_source_unique_indices[spatial_unique_cells[unique_idx].source_idx] = unique_idx;
  }

  spatial_hash.setCompactCells(
    spatial_unique_cells.data(),
    num_unique_cells,
    num_particles);

  {
    PROFILE_SCOPED_NAMED("parallelCellParticleMap");
    runInParallel(num_particles, num_partitions, [&](int start, int end, int job_id) {
      for (int particle_id = start; particle_id < end; ++particle_id) {
        const uint32_t bucket = assigned_cells[particle_id].cell_id & bucket_mask;
        const uint32_t source_unique_idx = spatial_bucket_unique_offsets[bucket] +
          spatial_particle_unique_indices[particle_id];
        const uint32_t unique_idx = spatial_source_unique_indices[source_unique_idx];
        spatial_hash.cells_per_vertex[particle_id] = {
          unique_idx,
          spatial_particle_indices_in_cell[particle_id]
        };
      }
      });
  }
}

void ViscoelasticSim::assignCellsHierarchical() {
  PROFILE_SCOPED_NAMED("assignCellsHierarchical");

  int macro_side = 4;
  if (spatial_hierarchy_macro_side <= 1)
    macro_side = 1;
  else if (spatial_hierarchy_macro_side <= 2)
    macro_side = 2;
  else if (spatial_hierarchy_macro_side >= 8)
    macro_side = 8;
  spatial_hierarchy_macro_side = macro_side;
  const bool xy_columns = spatial_hierarchy_xy_columns;
  const int local_cell_count = xy_columns
    ? macro_side * macro_side
    : macro_side * macro_side * macro_side;

  int num_buckets = 8;
  const int requested_buckets = std::max(8, std::min(spatial_index_buckets, 128));
  while (num_buckets < requested_buckets)
    num_buckets <<= 1;
  const uint32_t bucket_mask = (uint32_t)num_buckets - 1;
  const int num_partitions = std::max(1, std::min(num_threads, num_particles));
  const size_t partition_bucket_count = (size_t)num_partitions * num_buckets;

  auto macro_hash = [](const CPUSpatialSubdivision::Int3& coords) {
    uint32_t hash = static_cast<uint32_t>(coords.x) * 0x8da6b343u;
    hash ^= static_cast<uint32_t>(coords.y) * 0xd8163841u;
    hash ^= static_cast<uint32_t>(coords.z) * 0xcb1ab31fu;
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    return hash;
  };

  spatial_bucket_counts.assign(partition_bucket_count, 0);
  spatial_bucket_offsets.resize(partition_bucket_count);
  spatial_bucket_starts.resize((size_t)num_buckets + 1);
  spatial_bucket_particle_ids.resize(num_particles);
  spatial_bucket_unique_counts.resize(num_buckets);
  spatial_bucket_unique_offsets.resize((size_t)num_buckets + 1);
  hierarchy_particle_macro_coords.resize(num_particles);
  hierarchy_particle_local_ids.resize(num_particles);

  {
    PROFILE_SCOPED_NAMED("hierarchyMacroHistogram");
    runInParallel(num_particles, num_partitions, [&](int start, int end, int job_id) {
      uint32_t* counts = spatial_bucket_counts.data() + (size_t)job_id * num_buckets;
      for (int particle_id = start; particle_id < end; ++particle_id) {
        const auto& cell = assigned_cells[particle_id].ipos;
        int local_x = cell.x % macro_side;
        int local_y = cell.y % macro_side;
        if (local_x < 0) local_x += macro_side;
        if (local_y < 0) local_y += macro_side;
        assert(local_x >= 0 && local_x < macro_side);
        assert(local_y >= 0 && local_y < macro_side);

        int macro_z = 0;
        int local_z = 0;
        if (!xy_columns) {
          local_z = cell.z % macro_side;
          if (local_z < 0) local_z += macro_side;
          macro_z = (cell.z - local_z) / macro_side;
          assert(local_z >= 0 && local_z < macro_side);
        }
        const CPUSpatialSubdivision::Int3 macro = {
          (cell.x - local_x) / macro_side,
          (cell.y - local_y) / macro_side,
          macro_z
        };
        hierarchy_particle_macro_coords[particle_id] = macro;
        hierarchy_particle_local_ids[particle_id] = xy_columns
          ? (uint16_t)(local_y * macro_side + local_x)
          : (uint16_t)((local_y * macro_side + local_x) * macro_side + local_z);
        ++counts[macro_hash(macro) & bucket_mask];
      }
      });
  }

  {
    PROFILE_SCOPED_NAMED("hierarchyMacroPrefixScan");
    uint32_t particle_offset = 0;
    for (int bucket = 0; bucket < num_buckets; ++bucket) {
      spatial_bucket_starts[bucket] = particle_offset;
      for (int partition = 0; partition < num_partitions; ++partition) {
        const size_t idx = (size_t)partition * num_buckets + bucket;
        spatial_bucket_offsets[idx] = particle_offset;
        particle_offset += spatial_bucket_counts[idx];
      }
    }
    spatial_bucket_starts[num_buckets] = particle_offset;
    assert(particle_offset == (uint32_t)num_particles);
  }

  {
    PROFILE_SCOPED_NAMED("hierarchyMacroScatter");
    runInParallel(num_particles, num_partitions, [&](int start, int end, int job_id) {
      uint32_t* offsets = spatial_bucket_offsets.data() + (size_t)job_id * num_buckets;
      for (int particle_id = start; particle_id < end; ++particle_id) {
        const uint32_t bucket = macro_hash(hierarchy_particle_macro_coords[particle_id]) & bucket_mask;
        spatial_bucket_particle_ids[offsets[bucket]++] = (uint32_t)particle_id;
      }
      });
  }

  spatial_bucket_hash_offsets.resize((size_t)num_buckets + 1);
  uint32_t total_hash_capacity = 0;
  for (int bucket = 0; bucket < num_buckets; ++bucket) {
    spatial_bucket_hash_offsets[bucket] = total_hash_capacity;
    const uint32_t particle_count =
      spatial_bucket_starts[bucket + 1] - spatial_bucket_starts[bucket];
    uint32_t capacity = 0;
    if (particle_count > 0) {
      capacity = 2;
      while (capacity < particle_count * 2)
        capacity <<= 1;
    }
    total_hash_capacity += capacity;
  }
  spatial_bucket_hash_offsets[num_buckets] = total_hash_capacity;
  spatial_local_hash_coords.resize(total_hash_capacity);
  spatial_local_hash_unique_indices.resize(total_hash_capacity);
  spatial_particle_unique_indices.resize(num_particles);
  spatial_particle_indices_in_cell.resize(num_particles);
  hierarchy_provisional_macros.resize(num_particles);

  {
    PROFILE_SCOPED_NAMED("hierarchyGroupMacros");
    runInParallel(num_buckets, num_buckets, [&](int start, int end, int job_id) {
      for (int bucket = start; bucket < end; ++bucket) {
        const uint32_t hash_first = spatial_bucket_hash_offsets[bucket];
        const uint32_t hash_last = spatial_bucket_hash_offsets[bucket + 1];
        if (hash_first == hash_last) {
          spatial_bucket_unique_counts[bucket] = 0;
          continue;
        }

        std::fill(
          spatial_local_hash_unique_indices.begin() + hash_first,
          spatial_local_hash_unique_indices.begin() + hash_last,
          UINT32_MAX);
        const uint32_t local_hash_mask = hash_last - hash_first - 1;
        const uint32_t particle_first = spatial_bucket_starts[bucket];
        const uint32_t particle_last = spatial_bucket_starts[bucket + 1];
        uint32_t unique_count = 0;

        for (uint32_t i = particle_first; i < particle_last; ++i) {
          const uint32_t particle_id = spatial_bucket_particle_ids[i];
          const auto macro = hierarchy_particle_macro_coords[particle_id];
          uint32_t hash_slot = hash_first + (macro_hash(macro) & local_hash_mask);

          while (true) {
            uint32_t& local_macro_idx = spatial_local_hash_unique_indices[hash_slot];
            if (local_macro_idx == UINT32_MAX) {
              local_macro_idx = unique_count++;
              spatial_local_hash_coords[hash_slot] = macro;
              hierarchy_provisional_macros[particle_first + local_macro_idx] = {
                macro, 1, 0, 0, 0
              };
              spatial_particle_unique_indices[particle_id] = local_macro_idx;
              break;
            }

            if (spatial_local_hash_coords[hash_slot] == macro) {
              ++hierarchy_provisional_macros[particle_first + local_macro_idx].particle_count;
              spatial_particle_unique_indices[particle_id] = local_macro_idx;
              break;
            }
            hash_slot = hash_first + ((hash_slot - hash_first + 1) & local_hash_mask);
          }
        }
        spatial_bucket_unique_counts[bucket] = unique_count;
      }
      });
  }

  uint32_t num_macros = 0;
  for (int bucket = 0; bucket < num_buckets; ++bucket) {
    spatial_bucket_unique_offsets[bucket] = num_macros;
    num_macros += spatial_bucket_unique_counts[bucket];
  }
  spatial_bucket_unique_offsets[num_buckets] = num_macros;
  hierarchy_macros.resize(num_macros);
  hierarchy_source_macro_indices.resize(num_macros);

  {
    PROFILE_SCOPED_NAMED("hierarchySortMacros");
    for (int bucket = 0; bucket < num_buckets; ++bucket) {
      const uint32_t count = spatial_bucket_unique_counts[bucket];
      const uint32_t source = spatial_bucket_starts[bucket];
      const uint32_t destination = spatial_bucket_unique_offsets[bucket];
      for (uint32_t local_macro_idx = 0; local_macro_idx < count; ++local_macro_idx) {
        auto macro = hierarchy_provisional_macros[source + local_macro_idx];
        macro.source_idx = destination + local_macro_idx;
        hierarchy_macros[destination + local_macro_idx] = macro;
      }
    }
    std::sort(hierarchy_macros.begin(), hierarchy_macros.end(), [](const auto& a, const auto& b) {
      if (a.coords.y != b.coords.y)
        return a.coords.y < b.coords.y;
      if (a.coords.x != b.coords.x)
        return a.coords.x < b.coords.x;
      return a.coords.z < b.coords.z;
      });
    for (uint32_t macro_idx = 0; macro_idx < num_macros; ++macro_idx)
      hierarchy_source_macro_indices[hierarchy_macros[macro_idx].source_idx] = macro_idx;
  }
  hierarchy_macro_particle_offsets.resize((size_t)num_macros + 1);
  uint32_t macro_particle_offset = 0;
  for (uint32_t macro_idx = 0; macro_idx < num_macros; ++macro_idx) {
    HierarchyMacro& macro = hierarchy_macros[macro_idx];
    macro.particle_first = macro_particle_offset;
    hierarchy_macro_particle_offsets[macro_idx] = macro_particle_offset;
    macro_particle_offset += macro.particle_count;
  }
  hierarchy_macro_particle_offsets[num_macros] = macro_particle_offset;
  assert(macro_particle_offset == (uint32_t)num_particles);
  hierarchy_macro_particle_cursors.assign(
    hierarchy_macro_particle_offsets.begin(),
    hierarchy_macro_particle_offsets.end() - 1);
  hierarchy_macro_particle_ids.resize(num_particles);

  {
    PROFILE_SCOPED_NAMED("hierarchyScatterMacroParticles");
    runInParallel(num_buckets, num_buckets, [&](int start, int end, int job_id) {
      for (int bucket = start; bucket < end; ++bucket) {
        const uint32_t first = spatial_bucket_starts[bucket];
        const uint32_t last = spatial_bucket_starts[bucket + 1];
        for (uint32_t i = first; i < last; ++i) {
          const uint32_t particle_id = spatial_bucket_particle_ids[i];
          const uint32_t source_macro_idx = spatial_bucket_unique_offsets[bucket] +
            spatial_particle_unique_indices[particle_id];
          const uint32_t macro_idx = hierarchy_source_macro_indices[source_macro_idx];
          hierarchy_macro_particle_ids[hierarchy_macro_particle_cursors[macro_idx]++] = particle_id;
        }
      }
      });
  }

  hierarchy_macro_occupied_cell_counts.resize(num_macros);

  if (xy_columns) {
    hierarchy_macro_min_z.resize(num_macros);
    hierarchy_macro_z_spans.resize(num_macros);
    hierarchy_macro_sparse_fallbacks.resize(num_macros);
    hierarchy_macro_cell_table_offsets.resize((size_t)num_macros + 1);

    {
      PROFILE_SCOPED_NAMED("hierarchyPrepareXYColumns");
      runInParallel(num_macros, num_macros, [&](int start, int end, int job_id) {
        for (int macro_idx = start; macro_idx < end; ++macro_idx) {
          const uint32_t first = hierarchy_macro_particle_offsets[macro_idx];
          const uint32_t last = hierarchy_macro_particle_offsets[macro_idx + 1];
          int min_z = INT_MAX;
          int max_z = INT_MIN;
          for (uint32_t i = first; i < last; ++i) {
            const int z = assigned_cells[hierarchy_macro_particle_ids[i]].ipos.z;
            min_z = std::min(min_z, z);
            max_z = std::max(max_z, z);
          }

          assert(first < last);
          const uint64_t z_span = (uint64_t)((int64_t)max_z - min_z + 1);
          const uint64_t dense_slots = z_span * local_cell_count;
          // A normal liquid column has a compact Z extent and is faster as a
          // dense counting table. Keep a sort fallback for particles flung far
          // apart so a large empty Z interval cannot cause a huge allocation.
          const uint64_t dense_slot_limit = std::max<uint64_t>(
            (uint64_t)local_cell_count * 2,
            (uint64_t)(last - first) * 4);
          const bool sparse_fallback = dense_slots > dense_slot_limit ||
            dense_slots > UINT32_MAX;
          hierarchy_macro_min_z[macro_idx] = min_z;
          hierarchy_macro_z_spans[macro_idx] = sparse_fallback
            ? 0
            : (uint32_t)z_span;
          hierarchy_macro_sparse_fallbacks[macro_idx] = sparse_fallback;
        }
        });

      uint32_t total_dense_slots = 0;
      for (uint32_t macro_idx = 0; macro_idx < num_macros; ++macro_idx) {
        hierarchy_macro_cell_table_offsets[macro_idx] = total_dense_slots;
        const uint64_t macro_slots = (uint64_t)local_cell_count *
          hierarchy_macro_z_spans[macro_idx];
        assert(macro_slots <= UINT32_MAX - total_dense_slots);
        total_dense_slots += (uint32_t)macro_slots;
      }
      hierarchy_macro_cell_table_offsets[num_macros] = total_dense_slots;
      hierarchy_local_cell_counts.assign(total_dense_slots, 0);
      hierarchy_local_cell_ids.resize(total_dense_slots);
      hierarchy_local_cell_cursors.resize(total_dense_slots);
    }

    PROFILE_SCOPED_NAMED("hierarchyGroupXYColumns");
    runInParallel(num_macros, num_macros, [&](int start, int end, int job_id) {
      const auto cell_less = [&](uint32_t particle_a, uint32_t particle_b) {
        const auto& a = assigned_cells[particle_a].ipos;
        const auto& b = assigned_cells[particle_b].ipos;
        if (a.y != b.y)
          return a.y < b.y;
        if (a.x != b.x)
          return a.x < b.x;
        return a.z < b.z;
      };

      for (int macro_idx = start; macro_idx < end; ++macro_idx) {
        const uint32_t first = hierarchy_macro_particle_offsets[macro_idx];
        const uint32_t last = hierarchy_macro_particle_offsets[macro_idx + 1];
        uint32_t occupied = 0;
        if (hierarchy_macro_sparse_fallbacks[macro_idx]) {
          std::sort(
            hierarchy_macro_particle_ids.begin() + first,
            hierarchy_macro_particle_ids.begin() + last,
            cell_less);

          CPUSpatialSubdivision::Int3 previous;
          for (uint32_t i = first; i < last; ++i) {
            const auto coords = assigned_cells[hierarchy_macro_particle_ids[i]].ipos;
            if (i == first || !(coords == previous)) {
              ++occupied;
              previous = coords;
            }
          }
        }
        else {
          const uint32_t z_span = hierarchy_macro_z_spans[macro_idx];
          uint32_t* counts = hierarchy_local_cell_counts.data() +
            hierarchy_macro_cell_table_offsets[macro_idx];
          const int min_z = hierarchy_macro_min_z[macro_idx];
          for (uint32_t i = first; i < last; ++i) {
            const uint32_t particle_id = hierarchy_macro_particle_ids[i];
            const uint32_t local_xy = hierarchy_particle_local_ids[particle_id];
            const uint32_t local_z =
              (uint32_t)(assigned_cells[particle_id].ipos.z - min_z);
            assert(local_xy < (uint32_t)local_cell_count && local_z < z_span);
            ++counts[(size_t)local_xy * z_span + local_z];
          }
          for (uint32_t table_idx = 0;
               table_idx < (uint32_t)local_cell_count * z_span;
               ++table_idx) {
            occupied += counts[table_idx] != 0;
          }
        }
        hierarchy_macro_occupied_cell_counts[macro_idx] = occupied;
      }
      });
  }
  else {
    hierarchy_local_cell_counts.assign((size_t)num_macros * local_cell_count, 0);
    PROFILE_SCOPED_NAMED("hierarchyCountLocalCells");
    runInParallel(num_macros, num_macros, [&](int start, int end, int job_id) {
      for (int macro_idx = start; macro_idx < end; ++macro_idx) {
        uint32_t* counts = hierarchy_local_cell_counts.data() +
          (size_t)macro_idx * local_cell_count;
        const uint32_t first = hierarchy_macro_particle_offsets[macro_idx];
        const uint32_t last = hierarchy_macro_particle_offsets[macro_idx + 1];
        for (uint32_t i = first; i < last; ++i)
          ++counts[hierarchy_particle_local_ids[hierarchy_macro_particle_ids[i]]];
        uint32_t occupied = 0;
        for (int local_id = 0; local_id < local_cell_count; ++local_id)
          occupied += counts[local_id] != 0;
        hierarchy_macro_occupied_cell_counts[macro_idx] = occupied;
      }
      });
  }

  uint32_t num_unique_cells = 0;
  for (uint32_t macro_idx = 0; macro_idx < num_macros; ++macro_idx) {
    hierarchy_macros[macro_idx].cell_first = num_unique_cells;
    num_unique_cells += hierarchy_macro_occupied_cell_counts[macro_idx];
  }
  spatial_unique_cells.resize(num_unique_cells);

  if (xy_columns) {
    PROFILE_SCOPED_NAMED("hierarchyBuildXYColumns");
    runInParallel(num_macros, num_macros, [&](int start, int end, int job_id) {
      for (int macro_idx = start; macro_idx < end; ++macro_idx) {
        const HierarchyMacro& macro = hierarchy_macros[macro_idx];
        const uint32_t first = hierarchy_macro_particle_offsets[macro_idx];
        const uint32_t last = hierarchy_macro_particle_offsets[macro_idx + 1];
        uint32_t cell_idx = macro.cell_first;
        if (hierarchy_macro_sparse_fallbacks[macro_idx]) {
          uint32_t i = first;
          while (i < last) {
            const uint32_t first_particle_id = hierarchy_macro_particle_ids[i];
            const auto coords = assigned_cells[first_particle_id].ipos;
            uint32_t cell_last = i + 1;
            while (cell_last < last &&
                   assigned_cells[hierarchy_macro_particle_ids[cell_last]].ipos == coords)
              ++cell_last;

            const uint32_t particle_count = cell_last - i;
            spatial_unique_cells[cell_idx] = {
              coords,
              spatial_hash.gridHash(coords),
              particle_count,
              cell_idx
            };
            for (uint32_t particle_offset = 0;
                 particle_offset < particle_count;
                 ++particle_offset) {
              const uint32_t particle_id =
                hierarchy_macro_particle_ids[i + particle_offset];
              spatial_particle_unique_indices[particle_id] = cell_idx;
              spatial_particle_indices_in_cell[particle_id] = particle_offset;
            }
            ++cell_idx;
            i = cell_last;
          }
        }
        else {
          const uint32_t z_span = hierarchy_macro_z_spans[macro_idx];
          const uint32_t table_offset =
            hierarchy_macro_cell_table_offsets[macro_idx];
          const uint32_t* counts = hierarchy_local_cell_counts.data() + table_offset;
          uint32_t* cell_ids = hierarchy_local_cell_ids.data() + table_offset;
          uint32_t* cursors = hierarchy_local_cell_cursors.data() + table_offset;
          const int min_z = hierarchy_macro_min_z[macro_idx];

          for (int local_xy = 0; local_xy < local_cell_count; ++local_xy) {
            const int local_y = local_xy / macro_side;
            const int local_x = local_xy % macro_side;
            for (uint32_t local_z = 0; local_z < z_span; ++local_z) {
              const uint32_t table_idx = (uint32_t)local_xy * z_span + local_z;
              const uint32_t particle_count = counts[table_idx];
              if (particle_count == 0)
                continue;

              const CPUSpatialSubdivision::Int3 coords = {
                macro.coords.x * macro_side + local_x,
                macro.coords.y * macro_side + local_y,
                min_z + (int)local_z
              };
              cell_ids[table_idx] = cell_idx;
              cursors[table_idx] = 0;
              spatial_unique_cells[cell_idx] = {
                coords,
                spatial_hash.gridHash(coords),
                particle_count,
                cell_idx
              };
              ++cell_idx;
            }
          }

          for (uint32_t i = first; i < last; ++i) {
            const uint32_t particle_id = hierarchy_macro_particle_ids[i];
            const uint32_t local_xy = hierarchy_particle_local_ids[particle_id];
            const uint32_t local_z =
              (uint32_t)(assigned_cells[particle_id].ipos.z - min_z);
            const uint32_t table_idx = local_xy * z_span + local_z;
            spatial_particle_unique_indices[particle_id] = cell_ids[table_idx];
            spatial_particle_indices_in_cell[particle_id] = cursors[table_idx]++;
          }
        }
        assert(cell_idx == macro.cell_first +
          hierarchy_macro_occupied_cell_counts[macro_idx]);
      }
      });
  }
  else {
    PROFILE_SCOPED_NAMED("hierarchyBuildLocalCells");
    runInParallel(num_macros, num_macros, [&](int start, int end, int job_id) {
      for (int macro_idx = start; macro_idx < end; ++macro_idx) {
        const HierarchyMacro& macro = hierarchy_macros[macro_idx];
        const uint32_t* counts = hierarchy_local_cell_counts.data() +
          (size_t)macro_idx * local_cell_count;
        uint32_t local_cell_ids[kHierarchyMaxLocalCellCount];
        uint32_t local_starts[kHierarchyMaxLocalCellCount];
        uint32_t local_cursors[kHierarchyMaxLocalCellCount];
        uint32_t particle_offset = macro.particle_first;
        uint32_t cell_idx = macro.cell_first;

        for (int local_id = 0; local_id < local_cell_count; ++local_id) {
          local_starts[local_id] = particle_offset;
          local_cursors[local_id] = particle_offset;
          if (counts[local_id] == 0) {
            local_cell_ids[local_id] = UINT32_MAX;
            continue;
          }

          local_cell_ids[local_id] = cell_idx;
          const int local_y = local_id / (macro_side * macro_side);
          const int remainder = local_id % (macro_side * macro_side);
          const int local_x = remainder / macro_side;
          const int local_z = remainder % macro_side;
          const CPUSpatialSubdivision::Int3 coords = {
            macro.coords.x * macro_side + local_x,
            macro.coords.y * macro_side + local_y,
            macro.coords.z * macro_side + local_z
          };
          spatial_unique_cells[cell_idx] = {
            coords,
            spatial_hash.gridHash(coords),
            counts[local_id],
            cell_idx
          };
          particle_offset += counts[local_id];
          ++cell_idx;
        }
        assert(particle_offset == macro.particle_first + macro.particle_count);
        assert(cell_idx == macro.cell_first + hierarchy_macro_occupied_cell_counts[macro_idx]);

        const uint32_t first = hierarchy_macro_particle_offsets[macro_idx];
        const uint32_t last = hierarchy_macro_particle_offsets[macro_idx + 1];
        for (uint32_t i = first; i < last; ++i) {
          const uint32_t particle_id = hierarchy_macro_particle_ids[i];
          const uint32_t local_id = hierarchy_particle_local_ids[particle_id];
          const uint32_t destination = local_cursors[local_id]++;
          spatial_particle_unique_indices[particle_id] = local_cell_ids[local_id];
          spatial_particle_indices_in_cell[particle_id] = destination - local_starts[local_id];
        }
      }
      });
  }

  spatial_hash.setCompactCells(
    spatial_unique_cells.data(),
    num_unique_cells,
    num_particles);

  {
    PROFILE_SCOPED_NAMED("hierarchyMapParticles");
    runInParallel(num_particles, num_partitions, [&](int start, int end, int job_id) {
      for (int particle_id = start; particle_id < end; ++particle_id) {
        spatial_hash.cells_per_vertex[particle_id] = {
          spatial_particle_unique_indices[particle_id],
          spatial_particle_indices_in_cell[particle_id]
        };
      }
      });
  }
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

  if (spatial_hierarchy_audit.requested) {
    captureSpatialHierarchyAudit();
    spatial_hierarchy_audit.requested = false;
  }

  if (use_hierarchical_spatial_index)
    assignCellsHierarchical();
  else if (use_parallel_spatial_index)
    assignCellsParallel();
  else
    spatial_hash.setPoints(assigned_cells.data(), num_particles);

  {
    PROFILE_SCOPED_NAMED("sortParticles");
    runInParallel(num_particles, num_threads * sort_jobs_per_thread, [&](int start, int end, int job_id) {
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

}

void ViscoelasticSim::captureSpatialHierarchyAudit() {
  PROFILE_SCOPED_NAMED("captureSpatialHierarchyAudit");

  struct MacroCoord {
    int x, y, z;
    bool operator==(const MacroCoord& other) const {
      return x == other.x && y == other.y && z == other.z;
    }
  };
  struct MacroCoordHash {
    size_t operator()(const MacroCoord& coord) const {
      uint32_t hash = static_cast<uint32_t>(coord.x) * 0x8da6b343u;
      hash ^= static_cast<uint32_t>(coord.y) * 0xd8163841u;
      hash ^= static_cast<uint32_t>(coord.z) * 0xcb1ab31fu;
      hash ^= hash >> 16;
      hash *= 0x7feb352du;
      hash ^= hash >> 15;
      return (size_t)hash;
    }
  };
  struct MacroAccumulator {
    uint32_t particles = 0;
    uint32_t occupied_cells = 0;
    int min_z = INT_MAX;
    int max_z = INT_MIN;
    std::array<uint64_t, 8> occupied_local_cells = {};
  };

  auto floor_div = [](int value, int divisor) {
    int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder < 0)
      --quotient;
    return quotient;
  };
  auto percentile95 = [](const std::vector<int>& sorted_values) {
    if (sorted_values.empty())
      return 0;
    const size_t rank = ((sorted_values.size() * 95 + 99) / 100) - 1;
    return sorted_values[std::min(rank, sorted_values.size() - 1)];
  };

  SpatialHierarchyAudit& audit = spatial_hierarchy_audit;
  audit.valid = false;
  audit.xy_columns = spatial_hierarchy_xy_columns;
  audit.num_particles = num_particles;
  constexpr int macro_sides[] = { 1, 2, 4, 8 };

  for (int configuration_idx = 0; configuration_idx < 4; ++configuration_idx) {
    const int side = macro_sides[configuration_idx];
    const int fixed_local_cell_capacity = side * side * side;
    std::unordered_map<MacroCoord, MacroAccumulator, MacroCoordHash> macros;
    macros.reserve(std::max(1, num_particles / fixed_local_cell_capacity));
    std::unordered_map<MacroCoord, uint8_t, MacroCoordHash> occupied_cells;
    if (audit.xy_columns)
      occupied_cells.reserve(std::max(1, num_particles / 4));

    for (int particle_id = 0; particle_id < num_particles; ++particle_id) {
      const auto& cell = assigned_cells[particle_id].ipos;
      const MacroCoord macro = {
        floor_div(cell.x, side),
        floor_div(cell.y, side),
        audit.xy_columns ? 0 : floor_div(cell.z, side)
      };
      const int local_x = cell.x - macro.x * side;
      const int local_y = cell.y - macro.y * side;
      assert(local_x >= 0 && local_x < side);
      assert(local_y >= 0 && local_y < side);

      MacroAccumulator& accumulator = macros[macro];
      ++accumulator.particles;
      accumulator.min_z = std::min(accumulator.min_z, cell.z);
      accumulator.max_z = std::max(accumulator.max_z, cell.z);
      if (audit.xy_columns) {
        const MacroCoord cell_key = { cell.x, cell.y, cell.z };
        if (occupied_cells.emplace(cell_key, 0).second)
          ++accumulator.occupied_cells;
      }
      else {
        const int local_z = cell.z - macro.z * side;
        assert(local_z >= 0 && local_z < side);
        const int local_id = (local_y * side + local_x) * side + local_z;
        accumulator.occupied_local_cells[local_id >> 6] |=
          1ull << (local_id & 63);
      }
    }

    std::vector<int> particle_counts;
    std::vector<int> occupied_cell_counts;
    particle_counts.reserve(macros.size());
    occupied_cell_counts.reserve(macros.size());
    uint64_t total_particles = 0;
    uint64_t total_occupied_cells = 0;
    uint64_t total_local_table_slots = 0;

    for (const auto& entry : macros) {
      const MacroAccumulator& accumulator = entry.second;
      int macro_occupied_cells = (int)accumulator.occupied_cells;
      if (!audit.xy_columns) {
        const int num_words = (fixed_local_cell_capacity + 63) / 64;
        for (int word_idx = 0; word_idx < num_words; ++word_idx) {
#ifdef _MSC_VER
          macro_occupied_cells +=
            (int)__popcnt64(accumulator.occupied_local_cells[word_idx]);
#else
          macro_occupied_cells +=
            __builtin_popcountll(accumulator.occupied_local_cells[word_idx]);
#endif
        }
      }
      particle_counts.push_back((int)accumulator.particles);
      occupied_cell_counts.push_back(macro_occupied_cells);
      total_particles += accumulator.particles;
      total_occupied_cells += macro_occupied_cells;
      total_local_table_slots += audit.xy_columns
        ? (uint64_t)side * side *
          (uint64_t)((int64_t)accumulator.max_z - accumulator.min_z + 1)
        : fixed_local_cell_capacity;
    }

    std::sort(particle_counts.begin(), particle_counts.end());
    std::sort(occupied_cell_counts.begin(), occupied_cell_counts.end());
    auto& stats = audit.configurations[configuration_idx];
    stats = {};
    stats.side = side;
    stats.occupied_macros = (int)macros.size();
    stats.occupied_small_cells = (int)total_occupied_cells;
    stats.macros_per_worker = num_threads > 0 ? (float)macros.size() / (float)num_threads : 0.0f;
    if (!particle_counts.empty()) {
      stats.average_particles = (float)((double)total_particles / (double)particle_counts.size());
      stats.p95_particles = percentile95(particle_counts);
      stats.max_particles = particle_counts.back();
      stats.largest_particle_percent = num_particles > 0
        ? 100.0f * (float)stats.max_particles / (float)num_particles
        : 0.0f;
      stats.average_occupied_cells =
        (float)((double)total_occupied_cells / (double)occupied_cell_counts.size());
      stats.p95_occupied_cells = percentile95(occupied_cell_counts);
      stats.max_occupied_cells = occupied_cell_counts.back();
      stats.average_local_table_slots =
        (float)((double)total_local_table_slots / (double)occupied_cell_counts.size());
      stats.average_local_table_occupancy_percent =
        total_local_table_slots > 0
        ? 100.0f * (float)total_occupied_cells / (float)total_local_table_slots
        : 0.0f;
    }
  }

  audit.valid = true;
  audit.completed_this_update = true;
}

void ViscoelasticSim::cacheNearRanges(int cell_idx, bool capture_audit) {
  const auto& cell = spatial_hash.cells_ranges[cell_idx];
  auto& near_ranges = relaxation_near_ranges[cell_idx];
  spatial_hash.collectRanges(near_ranges, cell.cell_id);

  if (capture_audit)
    neighbour_range_counts_before_sort[cell_idx] = (uint8_t)near_ranges.n;

  if (use_hierarchical_spatial_index && sort_hierarchical_neighbour_ranges)
    sortAndCoalesceNearRanges(near_ranges);
}

void ViscoelasticSim::finishNeighbourRangeAudit() {
  NeighbourRangeAudit& audit = neighbour_range_audit;
  if (!audit.requested)
    return;

  audit.valid = true;
  audit.completed_this_update = true;
  audit.hierarchical = use_hierarchical_spatial_index;
  audit.xy_columns = use_hierarchical_spatial_index && spatial_hierarchy_xy_columns;
  audit.sorted_by_particle_offset =
    use_hierarchical_spatial_index && sort_hierarchical_neighbour_ranges;
  audit.macro_side = use_hierarchical_spatial_index ? spatial_hierarchy_macro_side : 0;
  audit.num_cells = (int)relaxation_near_ranges.size();
  audit.ranges_before = 0;
  audit.ranges_after = 0;
  audit.max_ranges_before = 0;
  audit.max_ranges_after = 0;

  for (int cell_idx = 0; cell_idx < audit.num_cells; ++cell_idx) {
    const int before = neighbour_range_counts_before_sort[cell_idx];
    const int after = (int)relaxation_near_ranges[cell_idx].n;
    audit.ranges_before += before;
    audit.ranges_after += after;
    audit.max_ranges_before = std::max(audit.max_ranges_before, before);
    audit.max_ranges_after = std::max(audit.max_ranges_after, after);
  }

  if (audit.num_cells > 0) {
    audit.average_ranges_before =
      (float)((double)audit.ranges_before / (double)audit.num_cells);
    audit.average_ranges_after =
      (float)((double)audit.ranges_after / (double)audit.num_cells);
  }
  else {
    audit.average_ranges_before = 0.0f;
    audit.average_ranges_after = 0.0f;
  }
  audit.requested = false;
}

void ViscoelasticSim::cacheRanges() {
  PROFILE_SCOPED_NAMED("cacheRanges");
  // The spatial hash is immutable here and each job writes a distinct range,
  // so the 27-cell neighbour lookups can be prepared independently.
  const int num_cells = (int)spatial_hash.cells_ranges.size();
  relaxation_near_ranges.resize(num_cells);
  const bool capture_audit = neighbour_range_audit.requested;
  if (capture_audit)
    neighbour_range_counts_before_sort.assign(num_cells, 0);
  runInParallel(num_cells, num_threads * cache_jobs_per_thread, [&](int start, int end, int job_id) {
    for (int cell_idx = start; cell_idx < end; ++cell_idx)
      cacheNearRanges(cell_idx, capture_audit);
    });
  if (capture_audit)
    finishNeighbourRangeAudit();
}

void ViscoelasticSim::updatePredictedPositionsRange(float dt, int start, int end) {
  PROFILE_SCOPED_NAMED("prepareParticles");
  const VEC3 delta_velocity = 0.02f * mat.kernel_radius * mat.gravity * dt;
  float attract_repel = attract ? 0.01f * mat.kernel_radius : 0.0f;
  attract_repel -= repel ? 0.01f * mat.kernel_radius : 0.0f;
  simd_prepare_particles(
    particles_pos,
    particles_prev_pos,
    particles_frozen_pos,
    particles_vels,
    particles_type,
    masses,
    delta_velocity,
    dt,
    in_2d,
    attract_repel,
    interact_point,
    interact_rad,
    start,
    end);
}

void ViscoelasticSim::updatePredictedPositions(float dt) {
  TTimer timer;
  const int num_jobs = std::max(1, std::min(prediction_jobs, num_threads));
  runInParallel(num_particles, num_jobs, [&](int start, int end, int job_id) {
    updatePredictedPositionsRange(dt, start, end);
    });
  saveTime(eSection::PredictPositions, timer);
}

void ViscoelasticSim::cacheRangesAndPredict(float dt) {
  PROFILE_SCOPED_NAMED("cacheRangesAndPredict");

  // Range caching depends only on the spatial hash produced above. Particle
  // preparation mutates independent array slices, so several preparation jobs
  // can share this heterogeneous phase with the cache jobs.
  const int num_cells = (int)spatial_hash.cells_ranges.size();
  relaxation_near_ranges.resize(num_cells);
  const bool capture_audit = neighbour_range_audit.requested;
  if (capture_audit)
    neighbour_range_counts_before_sort.assign(num_cells, 0);

  if (num_cells == 0) {
    updatePredictedPositions(dt);
    if (capture_audit)
      finishNeighbourRangeAudit();
    return;
  }

  const int num_cache_jobs = std::min(num_cells, num_threads * cache_jobs_per_thread);
  const int num_prediction_jobs = std::min(num_particles, std::max(1, std::min(prediction_jobs, num_threads)));
  const int chunk_size = (num_cells + num_cache_jobs - 1) / num_cache_jobs;
  const int prediction_chunk_size = (num_particles + num_prediction_jobs - 1) / num_prediction_jobs;
  std::atomic<int> cache_jobs_remaining{ num_cache_jobs };
  std::atomic<int> prediction_jobs_remaining{ num_prediction_jobs };
  TTimer cache_timer;
  TTimer prediction_timer;

  // Publish preparation first so several workers start its contiguous chunks
  // immediately. As each finishes, that worker automatically helps with the
  // remaining cache ranges.
  pool->dispatch(num_prediction_jobs + num_cache_jobs, [&](int job_id) {
    PROFILE_SCOPED_NAMED("C");
    if (job_id < num_prediction_jobs) {
      const int start = job_id * prediction_chunk_size;
      const int end = std::min(start + prediction_chunk_size, num_particles);
      updatePredictedPositionsRange(dt, start, end);
      if (prediction_jobs_remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
        saveTime(eSection::PredictPositions, prediction_timer);
      return;
    }

    PROFILE_SCOPED_NAMED("cacheRanges");
    const int cache_job_id = job_id - num_prediction_jobs;
    const int start = cache_job_id * chunk_size;
    const int end = std::min(start + chunk_size, num_cells);
    for (int cell_idx = start; cell_idx < end; ++cell_idx)
      cacheNearRanges(cell_idx, capture_audit);

    if (cache_jobs_remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
      saveTime(eSection::CacheRanges, cache_timer);
    });
  if (capture_audit)
    finishNeighbourRangeAudit();
}

void ViscoelasticSim::captureRelaxationAudit() {
  RelaxationAudit& audit = relaxation_audit;
  audit.valid = false;
  audit.num_workers = (int)relaxation_worker_deltas.size();
  audit.num_particles = num_particles;
  audit.total_delta_slots = (uint64_t)audit.num_workers * (uint64_t)num_particles;
  audit.nonzero_delta_slots = 0;
  audit.total_simd_blocks = 0;
  audit.active_simd_blocks = 0;
  audit.total_range_worker_pairs = 0;
  audit.active_range_worker_pairs = 0;
  audit.minmax_delta_slots = 0;
  audit.simd_aligned_minmax_delta_slots = 0;
  audit.active_workers = 0;
  audit.average_workers_per_particle = 0.0f;
  audit.max_workers_per_particle = 0;
  audit.average_workers_per_range = 0.0f;
  audit.min_workers_per_range = 0;
  audit.max_workers_per_range = 0;
  audit.neighbour_candidates_available = 0;
  audit.neighbour_candidates_checked = 0;
  audit.neighbour_candidates_accepted = 0;
  audit.neighbour_candidates_rejected = 0;
  audit.neighbour_candidates_discarded_by_cap = 0;
  audit.neighbour_candidates_skipped_by_cap = 0;
  audit.neighbour_simd_blocks_checked = 0;
  audit.neighbour_simd_blocks_active = 0;
  audit.particles_at_neighbour_cap = 0;

  constexpr int simd_width = 8;
  const int range_size = audit.range_size;
  const int num_ranges = (num_particles + range_size - 1) / range_size;
  const int num_simd_blocks = (num_particles + simd_width - 1) / simd_width;
  audit.workers_per_range.assign(num_ranges, 0.0f);
  audit.worker_range_coverage_percent.assign(audit.num_workers, 0.0f);
  audit.worker_min_touched_particle.assign(audit.num_workers, -1);
  audit.worker_max_touched_particle.assign(audit.num_workers, -1);
  audit.worker_nonzero_delta_slots.assign(audit.num_workers, 0);
  std::vector<uint16_t> workers_per_particle(num_particles, 0);

  audit.total_simd_blocks = (uint64_t)audit.num_workers * (uint64_t)num_simd_blocks;
  audit.total_range_worker_pairs = (uint64_t)audit.num_workers * (uint64_t)num_ranges;

  for (int worker_idx = 0; worker_idx < audit.num_workers; ++worker_idx) {
    const ParticlesVec& deltas = relaxation_worker_deltas[worker_idx];
    int worker_active_ranges = 0;
    int worker_min_touched = num_particles;
    int worker_max_touched = -1;
    uint64_t worker_nonzero_slots = 0;

    for (int range_idx = 0; range_idx < num_ranges; ++range_idx) {
      const int range_start = range_idx * range_size;
      const int range_end = std::min(range_start + range_size, num_particles);
      bool range_active = false;

      for (int block_start = range_start; block_start < range_end; block_start += simd_width) {
        const int block_end = std::min(block_start + simd_width, range_end);
        bool block_active = false;
        for (int particle_idx = block_start; particle_idx < block_end; ++particle_idx) {
          const bool nonzero = deltas.x[particle_idx] != 0.0f
            || deltas.y[particle_idx] != 0.0f
            || deltas.z[particle_idx] != 0.0f;
          if (nonzero) {
            ++audit.nonzero_delta_slots;
            ++worker_nonzero_slots;
            ++workers_per_particle[particle_idx];
            worker_min_touched = std::min(worker_min_touched, particle_idx);
            worker_max_touched = std::max(worker_max_touched, particle_idx);
            block_active = true;
          }
        }
        if (block_active) {
          ++audit.active_simd_blocks;
          range_active = true;
        }
      }

      if (range_active) {
        ++audit.active_range_worker_pairs;
        ++worker_active_ranges;
        audit.workers_per_range[range_idx] += 1.0f;
      }

    }

    if (num_ranges > 0)
      audit.worker_range_coverage_percent[worker_idx] = 100.0f * (float)worker_active_ranges / (float)num_ranges;

    audit.worker_nonzero_delta_slots[worker_idx] = worker_nonzero_slots;
    if (worker_max_touched >= 0) {
      audit.worker_min_touched_particle[worker_idx] = worker_min_touched;
      audit.worker_max_touched_particle[worker_idx] = worker_max_touched;
      ++audit.active_workers;

      const uint64_t span = (uint64_t)(worker_max_touched - worker_min_touched + 1);
      const int aligned_start = worker_min_touched & ~(simd_width - 1);
      const int aligned_end = std::min(num_particles, (worker_max_touched + simd_width) & ~(simd_width - 1));
      audit.minmax_delta_slots += span;
      audit.simd_aligned_minmax_delta_slots += (uint64_t)(aligned_end - aligned_start);
    }
  }

  if (num_particles > 0) {
    audit.average_workers_per_particle = (float)((double)audit.nonzero_delta_slots / (double)num_particles);
    for (uint16_t count : workers_per_particle)
      audit.max_workers_per_particle = std::max(audit.max_workers_per_particle, (int)count);
  }

  if (num_ranges > 0) {
    audit.min_workers_per_range = audit.num_workers;
    float total_workers_per_range = 0.0f;
    for (float count : audit.workers_per_range) {
      audit.min_workers_per_range = std::min(audit.min_workers_per_range, (int)count);
      audit.max_workers_per_range = std::max(audit.max_workers_per_range, (int)count);
      total_workers_per_range += count;
    }
    audit.average_workers_per_range = total_workers_per_range / (float)num_ranges;
  }

  // Replay only the neighbour acceptance test. This mirrors the eight-wide
  // blocks and the 64-neighbour cap without doing normalization or correction
  // math, and runs only for the explicitly requested audit frame.
  constexpr int max_nears = 64;
  const float radius_sq = mat.kernel_radius * mat.kernel_radius;
  for (size_t cell_idx = 0; cell_idx < spatial_hash.cells_ranges.size(); ++cell_idx) {
    const auto& cell_range = spatial_hash.cells_ranges[cell_idx];
    const auto& near_ranges = relaxation_near_ranges[cell_idx];

    uint64_t available_per_particle = 0;
    for (uint32_t range_idx = 0; range_idx < near_ranges.n; ++range_idx)
      available_per_particle += near_ranges.ranges[range_idx].last - near_ranges.ranges[range_idx].first;

    for (uint32_t i = cell_range.range.first; i < cell_range.range.last; ++i) {
      const float pi_x = particles_frozen_pos.x[i];
      const float pi_y = particles_frozen_pos.y[i];
      const float pi_z = particles_frozen_pos.z[i];
      int num_nears = 0;
      uint64_t checked_for_particle = 0;
      const uint64_t available_without_self = available_per_particle > 0 ? available_per_particle - 1 : 0;
      audit.neighbour_candidates_available += available_without_self;

      for (uint32_t range_idx = 0; range_idx < near_ranges.n && num_nears < max_nears; ++range_idx) {
        const uint32_t first = near_ranges.ranges[range_idx].first;
        const uint32_t last = near_ranges.ranges[range_idx].last;
        for (uint32_t j_start = first; j_start < last && num_nears < max_nears; j_start += 8) {
          ++audit.neighbour_simd_blocks_checked;
          const uint32_t lane_count = std::min(8u, last - j_start);
          int valid_in_block = 0;
          int candidates_in_block = 0;
          for (uint32_t lane = 0; lane < lane_count; ++lane) {
            const uint32_t j = j_start + lane;
            if (j == i)
              continue;
            ++candidates_in_block;
            const float dx = particles_frozen_pos.x[j] - pi_x;
            const float dy = particles_frozen_pos.y[j] - pi_y;
            const float dz = particles_frozen_pos.z[j] - pi_z;
            const float distance_sq = dx * dx + dy * dy + dz * dz;
            if (distance_sq < radius_sq && distance_sq > 1e-6f)
              ++valid_in_block;
          }

          if (valid_in_block > 0)
            ++audit.neighbour_simd_blocks_active;

          checked_for_particle += candidates_in_block;
          audit.neighbour_candidates_checked += candidates_in_block;
          audit.neighbour_candidates_rejected += candidates_in_block - valid_in_block;
          const int accepted_in_block = std::min(valid_in_block, max_nears - num_nears);
          audit.neighbour_candidates_accepted += accepted_in_block;
          audit.neighbour_candidates_discarded_by_cap += valid_in_block - accepted_in_block;
          num_nears += accepted_in_block;
        }
      }

      if (num_nears == max_nears)
        ++audit.particles_at_neighbour_cap;
      if (available_without_self > checked_for_particle)
        audit.neighbour_candidates_skipped_by_cap += available_without_self - checked_for_particle;
    }
  }

  audit.valid = true;
  audit.completed_this_update = true;
}

void ViscoelasticSim::doubleDensityRelaxationPara(float dt, ThreadPool& pool) {
  const int num_jobs = (int)spatial_hash.cells_ranges.size();

  // Each worker accumulates into its own full-sized buffer, so neighbour
  // scatters never contend. The reduction also clears each consumed delta,
  // leaving the buffers ready for the next pass without a separate phase.
  // More chunks keep faster cores useful near the end of the phase and limit
  // how much work a slower core can hold past the rest of the workers.
  runInParallel(num_jobs, num_threads * relaxation_jobs_per_thread, [&](int start, int end, int job_id) {
    ParticlesVec& worker_deltas = relaxation_worker_deltas[ThreadPool::currentWorkerIndex()];
    for (int cell_idx = start; cell_idx < end; ++cell_idx)
      processRange(dt, spatial_hash.cells_ranges[cell_idx], relaxation_near_ranges[cell_idx], particles_frozen_pos, &worker_deltas);
    });

  if (relaxation_audit.requested) {
    captureRelaxationAudit();
    relaxation_audit.requested = false;
  }

  runInParallel(num_particles, num_threads * relaxation_reduce_jobs_per_thread, [&](int start, int end, int job_id) {
    simd_apply_relaxation_deltas(particles_pos, relaxation_worker_deltas, start, end);
    });
}

void ViscoelasticSim::doubleDensityRelaxation(float dt) {
  for (size_t cell_idx = 0; cell_idx < spatial_hash.cells_ranges.size(); ++cell_idx)
    processRange(dt, spatial_hash.cells_ranges[cell_idx], relaxation_near_ranges[cell_idx], particles_frozen_pos, &particles_pos);
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

  if (overlap_cache_and_prediction) {
    cacheRangesAndPredict(dt);
  }
  else {
    {
      TTimer tm;
      cacheRanges();
      saveTime(eSection::CacheRanges, tm);
    }
    updatePredictedPositions(dt);
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
  relaxation_worker_deltas.resize(num_threads);
  for (ParticlesVec& deltas : relaxation_worker_deltas) {
    deltas.resize(max_particles);
    deltas.clearN(num_particles);
  }
  pool = new ThreadPool(num_threads);
}

void ViscoelasticSim::update(float delta_time) {
  relaxation_audit.completed_this_update = false;
  spatial_hierarchy_audit.completed_this_update = false;
  neighbour_range_audit.completed_this_update = false;
  // Keep workers hot across the short parallel phases of the simulation. They
  // park again before update returns, so rendering does not compete for CPU.
  pool->beginUpdate();
  sdf.generateCompactStructs();
  float dt = delta_time / (float)num_substeps;
  TTimer tm;
  for (int i = 0; i < num_substeps; ++i)
    updateStep(dt);
  pool->endUpdate();
  saveTime(eSection::Update, tm);
}
