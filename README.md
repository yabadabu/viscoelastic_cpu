# Introduction

This document describes the approach I have taken to perform a particle simulation in 3D using only the CPU's. The process allows to update 32K particles with collisions using 12 CPU's in just 4ms per update. Today it's more common to perform these type of simulations using the GPU, but I wanted to explore first the use of the CPU's.

[![Watch the video](results/sim00.png)](results/sim00.mp4)

The sample code focus on the simulation and uses a small C++ framework with DirectX 11 to draw a small sprite on each particle position. 

The simulation is based on the repository from https://github.com/kotsoft/particle_based_viscoelastic_fluid, and uses code from the following repo (all included):
- ImGui (https://github.com/ocornut/imgui),
- ImGuizmo (https://github.com/CedricGuillemet/ImGuizmo)
- ThreadPool (https://github.com/progschj/ThreadPool)

## Build and run in Windows

Open viscoelastic.sln with Visual C++ 2022 and press F5 in Release

## Particles

The simulation requires to store for each particle:
- position (3 floats)
- velocity (3 floats)
- prev_position (3 floats)
- cell_id, index_in_cell (2 ints)
- type (1 byte)

We will store each information in a separate linear buffer, using a SoA (Structure of Arrays) instead of AoS (Array of Structures). When we move to use sse/avx/avx2, even the 3 coords xyz of the vectors will be stored in separated linear buffers.

## Spatial Index

The objective is to be able to quickly find for each particle, all the particles nearby in a radius R, and have all the particles in each cell in a continuous region of memory.

For this we are going to split the 3D space in a regular grid of cells of fixed size. Each cell has 26 neighbours in 3D space.
We will identify each cell uniquely by its own 3D integer coordinates:

```cpp
    Int3 cell_coords = floor( pos * scale_factor );
```

![Particle Cells](results/particle_cells.png)

Because we don't know the 3D limits of our simulation, we will store the information for a limited number of cells, say 64K cells for example. 
We are going to generate a hash number for each cell_coords and use it to assign each coords to a planar array, using the lower bits of the hash.

```cpp
    uint32_t cell_id = hash( cell_coords );
```

Something like:

```cpp
    uint32_t hash( Int3 coords ) const {
      return ( (coords.x * prime1) ^ (coords.y * prime12) ^ (coords.z * prime3) ) & num_cells_mask;
    }
```

![Particle Cells in 2D](results/color_cells.png)

And in our case num_cells_mask = 64K - 1, so 0xffff

For each cell, we are going to store the following information: (u32 = unsigned int of 32 bits, and Int3 stores x3 ints)
```cpp
    struct CellInfo {
      u32  tag = 0;
      u32  count;		// Keep track of the number of particles associated to this cell in this frame
      Int3 coords;		// Current coords associated to this cell
      u32  base = 0;
    };
```
The tag is going to be an integer we will increase on each iteration of the simulation and allows us to recognize if the cell has been used in this frame or not.

We will need to deal with some hash collisions, when two cells with different cell_coords are assigned to the same cell_id number. In that case we will use a linear probing and use the next cell_id that it's available.

The algorithm is then:
1. Define a u32 current_tag, and increment it on each frame
2. For each particle.position, find the official cell_id:  onst CellInfo& cell = cell_infos[ cell_id ]
3. Check the CellInfo associated to the cell_id (array access):
  - If this is the first time we use this cell (comparing the tag vs current_tag):
    - We update the tag of the cell and reset the cell.count = 0 
    - We store in a separate array the cell_id as 'being used in this frame'
  - Otherwise, we need to confirm if the coords of the cell match the coords of our particle. If we need to find another cell, just increment the cell_id by one and try again. (Linear probing)
4. If the coords also match, the cell_id is good, increment the count of the cell_info struct.

In either case, save to cell_id and the current count in the cell that has been assigned to the particle.

This is not thread safe.

Once all the particles have been assigned a cell_id and the index in each particle, we also end with the list of cells (a selection of our big list of 64K cells) which 
contains particles. In my tests, we might use around 4000 particles of the 64K (6% approx).

We are free to 'sort' the cell_id's to our best interest. 

Then, we run the following code:

```cpp
    u32 acc = 0;
    for( u32 cell_id : used_cell_ids ) {
      cells[ cell_id ].base = acc;
      acc += cells[ cell_id ].count;
    }
```

Now we can run a final stage, where each particle is moved to a unique position	in a linear array.
```cpp
    for( u32 particle_id : num_particles ) {
      Particle p = old_particles[ particle_id ];
      u32 final_index = cells[ p.cell_id ].base + p.count_in_cell;
      new_particles[ final_particle_index ] = p;
    }
```
This stage can be run in parallel, as each particle already has a unique index associated.

At this point, we have a list of cells containing particles. Each cell has a base and count where we can access all the particles associated to the cell in a linear buffer.
For 32K particles, this takes about 1.4ms

## Simulation

- Apply external forces (gravity for example)
- Estimate new position
- Apply viscosity
- Apply results of viscosity
- Resolve collisions with walls
- Compute new_velocities

The huge cost goes to the apply viscosity, where for each particle we need to find the influence of all nearby particles.
For the viscositySolve to work, we make a copy of the positions of each particle, and accumulate the expected changes of each particle in a separate buffer, this way the we could run each particle in parallel without locking mechanisms

The code can perform substeps simulations but with just one step, the simulation is pretty stable.

## Collisions

The collisions will be handled using a collection of SDFs (Signed Distance Functions). In the current implementation we check each particle against an array of oriented planes, spheres and oriented boxes. The check affects only the position of the each individual particle which means we can run it in parallel using multiple threads.

With 6 planes we can define the interior of a box, and changing the orientation or the translation of the planes/spheres allows us to interact with the particles.

Right now, we check 32K particles vs 6 planes, and it takes xxx ms when running in parallel with 12 threads. Because most of the particles are not interacting with the walls, a posible optimization could consists of precomputing the list of sdf's that affect each cell, and check only the particles in those cells, as the spatial index already provides us with that information.

## Multithreading

Important considerations before going multithread:
- Avoid any locking or synchronization primitives at all cost, when possible.
  Even a single std::atomic<int> updated by all the threads generates a huge performance hit.
- We need to given a substancial amount of work to each thread to make sense. 
- When submitting a list of tasks to a pool of threads that were dormant, not all the threads start working immediately, and not 
  all the jobs require the same amount of time.
- Currently, some stages need to run from a single thread, like updating the spatial index.
- A simpler profiler is enough to confirm the usage of the CPU's

For example, If we need to update 32K particles with 12 threads, it's not a good idea to use a synchronization primitive like a std::atomic<int> for each particle to be updated. 
It's much better to split the 32K in jobs of for example, 1K particles, and let the threads pick one of the jobs and update those 1K continuous particles in a single shot.

There are two types of updates:

- Update all the particles where the update does not require neighbour information, like updating the position of each particle from the forces, update new velocity, etc. 
- Update all the particles based on the neighbours. Here we better update the particles by cell. So, get one cell, and process all the particles in each cell. Because the neighbours of the each particle in a cell are shared between all the particles, meaning the second particle will find the information in the local cache of the CPU.
	
Ideally, instead of giving just one thread, we better give a range of cells of each thread, if the range of cells are also close, we have even more chances to find the data already in the cache.
 
Remember that the spatial index we are using allows us to sort the cells by any criteria we want, and the particles in each cell are stored in continuous buffer.

## Results

For 32K particles, using 12 CPUs in a Ryzen Threadripper 3960X with 24-Cores, times in msecs

```
1.296 Spatial Hash
0.034 Velocities update
0.066 Predict Positions
3.042 Relaxation
0.229 Collisions
0.076 Velocities from positions
0.828 Render
4.908 Total update
```

And the thread utilizations during a single frame.

![CPU Profile](results/sim00.profile.png)

You can check the details openning the file `results/capture.json` using the `chrome://tracing/` url from Chrome. Or capture new traces using the `Profile Capture` button from the imgui

This is the tiem for the Relaxation stage as we increase the number of threads for the 32K particles simulation

<table>
  <tr>
    <td>
  
| # Threads  | Relaxation Time (secs)  |
|------------|-------------------------|
|         1  | 0.02353                 |
|         2  | 0.01225                 |
|         3  | 0.00841                 |
|         4  | 0.00582                 |
|         6  | 0.00471                 |
|         8  | 0.00386                 |
|        10  | 0.00332                 |
|        12  | 0.00295                 |
|        16  | 0.00235                 |
|        20  | 0.00212                 |
|        24  | 0.00185                 |

</td>
<td>

<img src="results/time_vs_threads.png" width="800"/>

</td>
  </tr>
</table>

And this is the time in secs with 12 threads as we increase the number of particles. Good point is that is scales linearly with the number of particles!

<table>
  <tr>
    <td>
	    
| # Particles | Relaxation Time  | Total Time  |
|-------------|-------------------------|--------------------|
|         1K  | 0,000245                | 0,000560           |
|         2K  | 0,000236                | 0,000670           |
|         4K  | 0,000310                | 0,000760           |
|         8K  | 0,000820                | 0,001490           |
|        12K  | 0,001123                | 0,001989           |
|        16K  | 0,001432                | 0,002612           |
|        20K  | 0,002021                | 0,003123           |
|        24K  | 0,002180                | 0,003650           |
|        28K  | 0,002627                | 0,004328           |
|        32K  | 0,002950                | 0,004810           |
|        36K  | 0,003415                | 0,005417           |
|        40K  | 0,003970                | 0,005980           |
|        44K  | 0,004013                | 0,006326           |
|        48K  | 0,004578                | 0,007125           |
|        52K  | 0,005215                | 0,007982           |
|        56K  | 0,005726                | 0,008523           |
|        60K  | 0,005928                | 0,008902           |
|        64K  | 0,005272                | 0,009523           |

</td>
<td>

<img src="results/time_vs_num_particles.png" width="800"/>

</td>
  </tr>
</table>

Finally, with 64K particles, increasing the number of threads brings some nice improvement, but using all threads does not.

| Num Threads | Total Time |
|-------------|-----------|
|         12  | 0,00952 |
|         24  | 0,00693 |
|         32  | 0,00612 |
|         48  | 0,00712 |

![Particle Cells in 2D](results/sim01.png)
![Particle Cells in 2D](results/sim02.png)

## Conclusions

- More threads does not mean better performance
- Multithreading pays off when enough independent work is pushed

## Improvements

- I have been testing an approach to generate the spatial index using multiple threads, but only pays off when more particles are being simulated
- The simulation is not fully viscoelastic as described in the original paper (https://dl.acm.org/doi/10.1145/1073368.1073400)
- Testing with different data alignments
- Testing with AVX512
- Test other CPU's
- Move it to GPU

## Multithread generation of the Spatial Index (uncomplete)

Generating the list of unique cell_id's and associate each particle with a unique position in a linear buffer, so that subsequent queries run in parallel is the purpose of this section. The proposed solution is the following:

- Each thread iterates over a range of particles.
- For each particle, compute the cell_id as normal
- Compute the group_id = cell_id & 7
- Each thread creates 8 groups, and stores each particle_id associated to each group
		groups[ group_id ].push_back( particle_id )
- At this points, each thread has organized the particles in 8 buckets
- Using a std::atomic<int> each thread allocates the number of particles for each group


