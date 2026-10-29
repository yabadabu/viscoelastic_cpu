# GPU Porting Guide

This document describes the current CPU simulation as a behavioural reference
for a compute-shader implementation using SDL3 and Vulkan. It is intentionally
about the simulation contract rather than the CPU implementation details.

The recommended approach is to reproduce the current behaviour first, validate
each stage, and only then introduce GPU-specific algorithm changes.

## Source of truth

The relevant CPU implementation is currently split across:

- `viscoelastic_sim.cpp`: simulation stages and equations.
- `viscoelastic_sim.h`: material parameters and persistent buffers.
- `cpu_spatial_subdivision.h`: grid construction and neighbouring-cell lookup.
- `particles_vec.h`: CPU structure-of-arrays particle storage.
- `engine/geometry/sdf/sdf.cpp`: collision SDF evaluation.
- `viscoelastic.cpp`: scenes, spawning, UI parameters, and benchmark setup.

The CPU code contains profiling, thread scheduling, SIMD, and bounded-grid
optimizations that should not be copied literally. The stage boundaries,
equations, buffer snapshots, and synchronization requirements should be kept.

## Current simulation in one diagram

One application update is divided into `num_substeps`. Each substep executes:

```text
start-of-substep particle state
        |
        v
build grid and reorder particles       grid uses start-of-substep positions
        |
        +-------------------------+
        |                         |
        v                         v
cache neighbouring cells     predict positions and apply gravity
                                  |
                                  +--> frozen_position = predicted position
                                  +--> position        = predicted position
        |                         |
        +------------ barrier ----+
                     |
                     v
find <= 64 neighbours and compute density/pressure
                     |
                     +--> neighbour IDs and counts
                     +--> pressure and near-pressure
                     |
                  barrier
                     |
                     v
pressure gather                       reads frozen positions
                     |                writes only its own position
                  barrier
                     |
                     v
SDF collisions and surface friction
                     |
                  barrier
                     |
                     v
reconstruct and clamp velocity
                     |
                  barrier
                     |
                     v
zero or more Jacobi viscosity iterations
                     |
                     v
rebuild previous_position from the final smoothed velocity
```

The CPU can overlap neighbouring-cell-range construction with prediction, but
this is only a scheduling optimization. A GPU implementation can initially run
them sequentially.

## Units and conventions

- Particle positions use simulation units.
- SDF primitives use world units.
- `world_scale` converts from SDF world space to particle space and defaults to
  `100.0`.
- The kernel radius `R` is in particle/simulation units and defaults to `20.0`.
- The normal 3D configuration uses a fixed `dt = 1.0` and one substep.
- The default maximum speed is `5.0` simulation units per unit time.
- Particle types are integers in `[0, 3]`.
- `masses[type]` currently scales gravity only. Mass is not used in density,
  pressure, or viscosity.
- A positive SDF value is valid/free space. A negative value means the particle
  penetrated a solid or escaped an inverted container.

The source defaults are:

```text
rest_density        = 4.0  (set to 3.0 by config3D_N)
stiffness           = 0.5
near_stiffness      = 0.5  (set to 1.0 by config3D_N)
viscosity           = 0.0  (normally tuned in the UI)
friction            = 0.0  (normally tuned in the UI)
kernel_radius       = 20.0
max_speed           = 5.0
world_scale         = 100.0
viscosity_iterations = 1
maximum neighbours  = 64
```

Record the desired viscosity and friction UI values before producing a strict
CPU/GPU comparison because the tuned values are not currently part of
`config3D_N`.

## Recommended GPU buffers

A simple first layout is:

```glsl
struct Particle {
    vec4 position;          // xyz used
    vec4 previousPosition;  // xyz used
    vec4 velocity;          // xyz used
    uint type;
};

struct PressurePair {
    float pressure;
    float nearPressure;
};
```

Suggested persistent and scratch buffers:

| Buffer | Elements | Purpose |
|---|---:|---|
| particle state A/B | `N` | Ping-pong state during grid reorder |
| frozen positions | `N` | Immutable predicted positions for relaxation |
| pressure pairs | `N` | Interleaved pressure and near-pressure |
| neighbour counts | `N` | Number of accepted neighbours, `0..64` |
| neighbour IDs | `N * 64` | Fixed-stride neighbour rows |
| cell counts | number of cells | Grid histogram |
| cell offsets | number of cells + 1 | Exclusive prefix sum |
| cell cursors | number of cells | Scatter counters |
| sorted particle IDs or reordered state | `N` | Particles grouped by cell |
| viscosity velocity A/B | `N` | Jacobi ping-pong buffers |
| SDF primitives | scene dependent | Planes, spheres, and oriented boxes |

Using `vec4` for position and velocity is usually preferable on the GPU even
though the CPU uses separate X/Y/Z arrays. Measure before attempting a GPU SoA
layout. The pressure pair should remain interleaved because both values are
consumed together.

At 128K particles, the fixed neighbour-ID buffer occupies approximately 32 MiB:

```text
128 * 1024 particles * 64 neighbours * 4 bytes = 32 MiB
```

That cost is useful initially because the same list is consumed by pressure
gather and viscosity. Recomputing neighbours is a later performance tradeoff.

## Stage 1: spatial grid

### CPU behaviour

The grid is built from the positions at the **start of the substep**, before
gravity and prediction.

The default cell dimensions are:

```text
cell_size = (R, R, R)
```

An experimental CPU mode uses `(R, R, R/2)` and therefore checks two Z cells in
each direction. Start the GPU port with cubic `R` cells and a 3x3x3 candidate
stencil.

The CPU reorders all particle streams so particles in a cell are contiguous.
Cells are ordered by Y, then X, then Z. The optimized bounded implementation
groups XY columns and orders each column by integer Z cell and then exact Z.

### Recommended first GPU implementation

The Platforms scene has practical bounds, so a dense bounded 3D grid is easier
to implement and validate than the CPU hash table:

1. Clear `cellCounts`.
2. Compute one integer cell coordinate per particle.
3. Atomically increment each cell count.
4. Exclusive-scan `cellCounts` into `cellOffsets`.
5. Copy offsets into scatter cursors.
6. Scatter particles or particle IDs into cell-contiguous storage.

A dense grid avoids hash collisions and makes the 27 neighbouring cells direct
array lookups. If the simulation later needs unbounded space, replace this with
a sorted cell-key/radix-sort implementation without changing the remaining
simulation stages.

For locality, dispatch relaxation over the cell-sorted particle order. Either:

- reorder the complete particle state into a ping-pong buffer, matching the CPU;
  or
- retain stable particle storage and scatter only IDs.

Reordering the complete state is the closer CPU match and gives more coherent
reads. Stable IDs are simpler for external object tracking. The renderer does
not currently require stable particle identities.

### Important ordering issue

The 64-neighbour limit makes traversal order part of the algorithm. If more
than 64 valid neighbours exist, a different scatter order can select a
different subset and therefore change density, pressure, and viscosity.

For initial parity, record how often the cap is reached and try to keep a
repeatable cell and particle order. Do not expect bitwise equality from an
atomic scatter. If cap hits are significant, possible later policies are:

- select the nearest 64 neighbours;
- increase the cap;
- use all neighbours without storing a fixed list.

Those policies change the simulation and should be evaluated separately.

## Stage 2: prediction

For each particle `i`, read its start-of-substep state:

```text
gravity_delta = 0.02 * R * gravity * dt
velocity_i   += gravity_delta * masses[type_i]
predicted_i   = position_i + velocity_i * dt

previous_position_i = position_i
frozen_position_i   = predicted_i
position_i          = predicted_i
```

`frozen_position` is an immutable snapshot used by both pressure passes. The
pressure gather must not read positions already corrected by other invocations.

Interactive attraction/repulsion and 2D mode are optional features. Port them
after the 3D simulation agrees.

The CPU builds the grid before this stage. Rebuilding the grid from predicted
positions may be more natural on the GPU, but it is an algorithm change. Keep
the CPU ordering for the parity version.

## Stage 3: neighbour collection, density, and pressure

Run one invocation per particle. Inspect particles from its 27 candidate cells.

A candidate `j` is accepted when:

```text
j != i
distance_squared < R * R
distance_squared > 1e-6
accepted_count < 64
```

Distances use `frozen_position`:

```text
delta = frozen_position[j] - frozen_position[i]
r     = length(delta) + 1e-5
c     = 1 - r / R
```

Accumulate:

```text
density      += c * c
near_density += c * c * c
```

Store each accepted `j` in:

```text
neighbourIds[i * 64 + accepted_count]
```

Then compute:

```text
pressure_i = min(
    1,
    stiffness * dt * dt * (density - rest_density)
)

near_pressure_i = min(
    1,
    near_stiffness * dt * dt * max(0, near_density)
)
```

There is deliberately no lower clamp on `pressure`. Density below rest density
therefore produces negative pressure. `near_pressure` is non-negative.

Write pressure and near-pressure adjacently. A compute-to-compute storage
barrier is required before pressure gather because every invocation can read
the pressure produced by every neighbouring invocation.

## Stage 4: pressure gather

Run one invocation per particle. It reads only:

- immutable `frozen_position`;
- all pressure pairs;
- its own neighbour row.

It writes only `position[i]`. Therefore no atomics or cross-particle writes are
required.

For every saved neighbour `j`:

```text
delta  = frozen_position[j] - frozen_position[i]
r      = length(delta) + 1e-5
inv_r  = 1 / r
c      = 1 - r / R

amount = 0.5 * c * (
    pressure_i + pressure_j
    + (near_pressure_i + near_pressure_j) * c
)

correction_i -= delta * (inv_r * amount)
```

After all neighbours:

```text
position[i] += correction_i
```

Equivalent shader-style pseudocode:

```glsl
vec3 correction = vec3(0.0);
for (uint k = 0; k < neighbourCount[i]; ++k) {
    uint j = neighbourIds[i * 64 + k];
    vec3 delta = frozenPosition[j].xyz - frozenPosition[i].xyz;
    float r = length(delta) + 1e-5;
    float c = 1.0 - r * inverseKernelRadius;
    float amount = 0.5 * c * (
        pressure[i].x + pressure[j].x +
        (pressure[i].y + pressure[j].y) * c);
    correction -= delta * (amount / r);
}
position[i].xyz += correction;
```

This gather formulation is the most important GPU-friendly property of the
current solver: there are no pairwise scatter writes and no float atomics.

## Stage 5: SDF collisions and surface friction

The CPU scene supports planes, spheres, and oriented boxes. Their signed
distances are combined using `min`.

For each particle:

```text
q = position[i] / world_scale
d = scene_sdf(q)
```

If `d < 0`, calculate the normalized SDF gradient. The CPU currently uses
central differences with `epsilon = 0.01` world units:

```text
grad.x = sdf(q + (eps, 0, 0)) - sdf(q - (eps, 0, 0))
grad.y = sdf(q + (0, eps, 0)) - sdf(q - (0, eps, 0))
grad.z = sdf(q + (0, 0, eps)) - sdf(q - (0, 0, eps))
grad   = normalize(grad)
```

Apply the current correction exactly as:

```text
boundary_multiplier = -0.5 * dt * dt * world_scale
position[i] += grad * d * boundary_multiplier
```

Then apply surface friction by moving the previous position toward the corrected
position:

```text
previous_position[i] +=
    (position[i] - previous_position[i]) * friction
```

Because velocity is reconstructed later, this scales the post-collision motion
by approximately `1 - friction`. With `friction = 1`, the reconstructed motion
at the contact becomes zero.

For parity, use the numerical gradient first. Analytical gradients for each SDF
primitive can be a later optimization; six full scene evaluations per colliding
particle can otherwise be expensive.

## Stage 6: velocity reconstruction

After pressure correction and collision handling:

```text
velocity[i] = (position[i] - previous_position[i]) / dt
```

Clamp the complete vector to `max_speed`:

```text
if length(velocity[i]) > max_speed:
    velocity[i] = normalize(velocity[i]) * max_speed
```

The CPU SIMD code uses an approximate reciprocal during normalization, so small
CPU/GPU differences are expected.

## Stage 7: viscosity

Viscosity reuses the pressure-stage neighbour IDs, but recomputes weights from
the final, post-collision positions.

The per-iteration blend is:

```text
iteration_dt = dt / viscosity_iterations
blend = min(0.5, 1 - exp(-viscosity * iteration_dt))
```

For each particle and saved neighbour:

```text
delta = position[j] - position[i]
distance_squared = dot(delta, delta)
s = max(0, 1 - distance_squared / (R * R))
weight = s * s
```

Calculate the normalized weighted neighbour velocity:

```text
average_velocity = sum(velocity[j] * weight) / sum(weight)
```

If the weight sum is greater than `1e-6`:

```text
velocity_out[i] = mix(velocity_in[i], average_velocity, blend)
```

Otherwise copy the input velocity.

This is a Jacobi operation: every invocation reads the same input velocity
buffer and writes a separate output buffer. Swap buffers after every iteration
and insert a compute storage barrier before the next iteration.

After the final iteration, restore the position/velocity invariant:

```text
previous_position[i] = position[i] - velocity[i] * dt
```

## Vulkan synchronization

The complete port can remain in one command buffer. It does not require a queue
submission between compute stages. Use `vkCmdPipelineBarrier2` between stages
whose shader-storage writes are consumed by later compute dispatches.

Conceptually, each dependency is:

```text
source stage/access: COMPUTE_SHADER / SHADER_STORAGE_WRITE
dest stage/access:   COMPUTE_SHADER / SHADER_STORAGE_READ
```

When the next stage also updates the same buffer, include storage-write access
on the destination side as appropriate.

Required logical barriers include:

1. Cell histogram -> prefix scan.
2. Prefix/cursors -> particle scatter.
3. Scatter/reorder -> prediction and neighbour lookup.
4. Prediction -> density/pressure.
5. Pressure writes -> pressure gather.
6. Pressure gather -> collision pass.
7. Collision pass -> velocity reconstruction.
8. Velocity reconstruction -> viscosity iteration 0.
9. Every viscosity ping-pong iteration -> the next iteration.
10. Final simulation writes -> vertex/graphics shader reads for rendering.

The exact Vulkan masks depend on whether buffers are read as storage buffers,
vertex buffers, or through device addresses. Validate the synchronization setup
with Vulkan validation layers enabled.

## Suggested compute pipelines

A clean first implementation can use these shaders:

```text
clear_grid.comp
count_cells.comp
scan_cells.comp
prepare_scatter.comp
scatter_particles.comp
predict.comp
density_pressure.comp
pressure_gather.comp
collisions.comp
reconstruct_velocity.comp
viscosity.comp
rebuild_previous_position.comp
```

The scan may require multiple dispatches for block sums. Later, compatible
stages can be fused if profiling shows that bandwidth or dispatch overhead is
important.

Do not fuse density/pressure with pressure gather: gather needs a global barrier
after all particle pressures have been produced.

## SDF representation on the GPU

The CPU converts editable primitives into compact arrays once per application
update:

- plane: normalized normal, plane constant, multiplier;
- sphere: center, radius, multiplier;
- oriented box: world-to-local matrix, softness, multiplier.

Upload equivalent compact arrays when a scene changes. The scene distance is:

```text
min(all enabled primitive signed distances)
```

A negative multiplier creates an interior container, as used by the Inside Box
and Inside Sphere scenes.

The Platforms scene is the most useful initial integration test because it has
enough volume for 128K particles and exercises planes plus rotated boxes.

## Behavioural validation strategy

Do not begin validation with 128K particles. Build confidence in layers.

### 1. Equation tests without a grid

Use a brute-force GPU neighbour loop for a very small particle count. Validate:

- one particle under gravity;
- two particles inside the kernel radius;
- two particles outside the radius;
- a symmetric cluster with gravity disabled;
- one particle penetrating a plane;
- two different velocities with viscosity enabled.

This separates shader-equation errors from grid-construction errors.

### 2. Validate grid membership

For a deterministic particle set, read back:

- integer cell coordinate;
- cell count and offset;
- sorted particle IDs;
- candidate-cell ranges.

Confirm that every particle appears exactly once and every accepted neighbour
comes from the expected 27-cell stencil.

### 3. Compare stage outputs

For a small deterministic scene, compare CPU and GPU snapshots after:

1. prediction;
2. neighbour collection and pressure;
3. pressure gather;
4. collision;
5. velocity reconstruction;
6. viscosity.

Useful per-stage diagnostics are:

```text
maximum absolute position error
RMS position error
maximum velocity error
mean and maximum neighbour count
number of particles reaching the 64-neighbour cap
minimum/maximum density
minimum/maximum pressure
number of NaN or infinite components
```

Bitwise equality is not a realistic requirement because parallel reductions,
FMA contraction, reciprocal precision, and neighbour ordering differ. Large or
systematic divergence in the first few substeps is still an actionable error.

### 4. Visual regression

Use the Platforms scene with the deterministic initial spawn:

```text
seed: 54123
x: [50, 850]
y: [50, 900]
z: [-500, 250]
initial velocity: (0, 0, 0)
```

If the two frameworks use different random-number generators, export the CPU
initial positions once rather than expecting the same seed to generate the same
sequence.

Compare early frames before chaotic floating-point divergence dominates. Then
compare qualitative behaviour: volume, compressibility, settling, surface
drag, breakup, and viscosity.

## Performance validation

Use GPU timestamp queries around individual compute stages and around the whole
simulation. Avoid timing command recording or a forced CPU wait as GPU work.
Read timestamp results several frames later through a ring of query slots.

Keep separate measurements for:

- grid clear/count/scan/scatter;
- prediction;
- density and neighbour-list generation;
- pressure gather;
- collisions;
- velocity reconstruction;
- viscosity;
- rendering.

For comparison with the CPU benchmark, use:

- Platforms scene;
- 32K, 64K, and 128K particles;
- 300 warm-up frames;
- 300 measured frames;
- average simulation update time.

Also record GPU model, driver version, commit ID, particle count, substeps,
viscosity iterations, and all material parameters. CPU thread count has no GPU
equivalent and should remain a CPU-only metadata field.

## Parity first, optimization second

Keep these behaviours during the first implementation:

- grid built from start-of-substep positions;
- immutable frozen predicted positions for both pressure passes;
- at most 64 accepted neighbours;
- density and near-density calculated from that accepted list;
- interleaved pressure pair;
- gather-only pressure correction;
- collision before velocity reconstruction;
- viscosity after velocity reconstruction;
- Jacobi viscosity ping-pong;
- previous position rebuilt after viscosity.

Potential later GPU experiments include:

- build the grid from predicted positions;
- recompute neighbours instead of storing 32 MiB of IDs;
- select nearest neighbours rather than first encountered neighbours;
- remove or raise the neighbour cap;
- use subgroup operations for neighbour accumulation;
- process one cell or XY column per workgroup;
- cache a neighbouring particle tile in shared memory;
- use analytical SDF gradients;
- fuse lightweight particle-only stages;
- use half precision only for proven low-sensitivity data;
- render directly from the final storage buffer without a copy.

Each experiment should be benchmarked and visually evaluated independently.

## Initial implementation checklist

- [ ] Create aligned particle and parameter GPU structs shared with GLSL.
- [ ] Upload a deterministic initial particle snapshot.
- [ ] Implement prediction without a grid.
- [ ] Implement brute-force density and pressure for a small test.
- [ ] Implement pressure gather and compare stage snapshots.
- [ ] Implement compact SDF primitives and collision response.
- [ ] Implement velocity reconstruction and speed clamp.
- [ ] Implement viscosity ping-pong and previous-position rebuild.
- [ ] Add the dense grid histogram, scan, and scatter.
- [ ] Replace brute-force neighbours with the 27-cell search.
- [ ] Add cap-hit, NaN, and per-stage diagnostic counters.
- [ ] Add Vulkan timestamp queries.
- [ ] Render directly from the final GPU particle positions.
- [ ] Reproduce the Platforms scene at 32K, 64K, and 128K particles.
- [ ] Only then begin GPU-specific algorithm changes.

## Core invariants

When debugging, check these before tuning performance:

1. Each active particle is present in exactly one grid cell.
2. Every saved neighbour ID is valid, not self, and was within `R` during the
   density pass.
3. `neighbourCount[i] <= 64` for every particle.
4. Pressure gather reads frozen positions, never partially corrected positions.
5. Each gather invocation writes only its own particle.
6. Each viscosity iteration reads one velocity buffer and writes the other.
7. After viscosity, `previous_position = position - velocity * dt`.
8. Position, previous position, velocity, pressure, and density remain finite.
9. Graphics does not read particle positions until the final compute writes are
   visible.

These invariants are more useful than attempting to match the CPU's exact
floating-point bit pattern.
