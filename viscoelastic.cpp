#include "platform.h"
#include "render/render.h"
#include "render/debug_texts.h"
#include "viscoelastic_sim.h"

extern VEC2 mouse_cursor;

struct ViscoelasticModule : public Module {
  const char* getName() const override { return "viscoelastic"; }

  ViscoelasticSim          sim;
  
  struct Emitter {
    TTransform               transform;
    float                    radius = 0.1f;
    float                    strength = 20.0f;
    int                      rate = 4;
    bool                     enabled = false;
    int                      particle_type = 0;
    int                      num_pendings = 0;

    int                      counter = 0;
    int                      period = 1024;

    enum eGenerationType {
      eUniform,
      eRandom,
      eRanges
    };
    eGenerationType generation_type = eGenerationType::eRanges;

    bool renderInMenu() {
      if (!ImGui::TreeNode("Emitter..."))
        return false;
      bool changed = transform.debugInMenu();

      ImGui::Text("Queue");
      ImGui::SameLine();
      if (ImGui::SmallButton("100"))
        add(100);
      ImGui::SameLine();
      if (ImGui::SmallButton("1024"))
        add(1024);
      ImGui::SameLine();
      if (ImGui::SmallButton("4096"))
        add(4096);

      ImGui::Checkbox("Emitting", &enabled);
      if (num_pendings) {
        ImGui::SameLine();
        ImGui::Text("%d", num_pendings);
      }
      ImGui::DragFloat("Radius", &radius, 0.01f, 0.1f, 3.0f);
      ImGui::DragFloat("Strength", &strength, 0.1f, 1.0f, 35.0f);
      ImGui::DragInt("Rate", &rate, 0.1f, 0, 32);
      ImGui::Combo("Generation Type", (int*)&generation_type, "Uniform\0Random\0Ranges\0\0", 4);
      ImGui::DragInt("Type", &particle_type, 0.05f, 0, 3);
      ImGui::TreePop();
      return changed;
    }
    void add(int n) {
      num_pendings += n;
      enabled = true;
    }
    void emit(ViscoelasticSim& sim) {
      if (!enabled)
        return;
      static TRandomSequence rseq;
      for (int i = 0; i < rate; ++i) {
        VEC3 p = transform.transformCoord(VEC3(rseq.between(-radius, radius), rseq.between(-radius, radius), rseq.between(-radius, radius))) * sim.world_scale;
        VEC3 v = transform.transformDir(VEC3::axis_z) * strength;
        if (generation_type == eGenerationType::eRandom)
          particle_type = rseq.between(0, 4);
        else if( generation_type == eGenerationType::eRanges ) {
          counter += 1;
          if (counter > period) {
            counter = 0;
            particle_type = ( particle_type + 1 ) % 4;
          }
        }
        sim.addParticle(p, v, particle_type);
      }
      if (num_pendings > 0) {
        num_pendings -= rate;
        if (num_pendings <= 0)
          enabled = false;
      }
    }
  };

  Emitter                  emitter;

  bool                     paused = false;
  bool                     auto_pause = false;
  float                    delta_time = 1.0;
  float                    gravity_direction = -90.0f;
  float                    gravity_amount = 0.1f;

  Render::VSpriteInstances sprites;
  Render::VInstances       wired_cells;
  Render::VInstances       lines;

  double                   time_render = 0.0f;

  VEC4                     colors[4] = { Color::Red, Color::Green, Color::Blue, Color::White };
  int                      num_particles_m0 = 2048;
  int                      num_particles_m1 = 2048;
  int                      num_particles_m2 = 2048;

  void updateParticleTypes() {
    int counters[3];
    counters[0] = num_particles_m0;
    counters[1] = counters[0] + num_particles_m1;
    counters[2] = counters[1] + num_particles_m2;
    for (int i = 0; i < sim.num_particles; ++i) {
      u8 idx = 0;
      if (i > counters[0])
        idx = 1;
      if (i > counters[1])
        idx = 2;
      if (i > counters[2])
        idx = 3;
      sim.particles_type[i] = idx;
    }
  }

  bool        use_cell_colors = false;
  bool        show_cells = false;
  bool        show_ids = false;
  bool        show_cell_ids = false;
  bool        auto_rotate_first_box = false;
  float       auto_rotation_speed = 1.0f;     // in degs
  CDebugTexts dbg_texts;

  int       debug_particle = -1;

  ViscoelasticModule() {
    sim.init();
    sim.in_2d = true;
    //sim.sdf.prims.push_back(SDF::Primitive::makeBox(VEC3(0, 1.5, 1.0), VEC3(1.0f, 2.0f, 3.0f) * 0.1f));
    emitter.transform.setPosition(VEC3(0.0f, 3.0f, 1.0f));
    sdfCage();
    addParticles(512);
    //sdfLargeCage();
    //config3D_32K();
  }

  void sdfCage(  ) {
    sim.sdf.prims.clear();
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3::zero, VEC3::axis_y));
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(0, 6, 0), -VEC3::axis_y));
    const float sz = 2.5f;
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(0, 0, sz), -VEC3::axis_z));
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(0, 0, 0), VEC3::axis_z));
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(sz, 0, 0), -VEC3::axis_x));
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(0, 0, 0), VEC3::axis_x));
  }

  void sdfInsideCage( ) {
    sim.sdf.prims.clear();
    sim.sdf.prims.push_back(SDF::Primitive::makeBox(VEC3(0.0f, 5.0f, 3.0f), VEC3( 4.0f, 2.0, 2.0f )));
    sim.sdf.prims.back().multiplier = -1.0f;
    sim.sdf.prims[3].transformHasChanged();
  }

  void sdfLargeCage() {
    sim.in_2d = false;
    sdfCage();
    sim.sdf.prims[3].transform.position.z = -5.0;
    sim.sdf.prims[3].transformHasChanged();
  }

  void sdfPlatforms() {
    sim.in_2d = false;
    sdfLargeCage();
    emitter.transform.setPosition(VEC3(0.0f, 5.7f, -1.5f));
    sim.sdf.prims.push_back(SDF::Primitive::makeBox(VEC3(1.0f, 2.5f, -2.5f), VEC3(10.0f, 2.0f, 10.0f) * 0.2f));
    sim.sdf.prims.back().transform.setRotation(QUAT::createFromAxisAngle(VEC3::axis_x, deg2rad(20.0f)));
    sim.sdf.prims.back().transformHasChanged();
    sim.sdf.prims.push_back(SDF::Primitive::makeBox(VEC3(1.0f, 4.5f, 1.0f), VEC3(10.0f, 2.0f, 10.0f) * 0.2f));
    sim.sdf.prims.back().transform.setRotation(QUAT::createFromAxisAngle(VEC3::axis_x, deg2rad(-20.0f)));
    sim.sdf.prims.back().transformHasChanged();
  }

  void load() override {
    wired_cells = Render::VInstances("unit_wired_cube.mesh");
    lines = Render::VInstances( "line.mesh" );
  }

  void addParticles(int num) {
    uint8_t particle_type = 0;
    TRandomSequence seq(54123);
    for (int i = 0; i < num; ++i) {
      if (sim.in_2d)
        sim.addParticle(VEC3(0.0f, seq.between(200.0, 1000.0f), seq.between(-500.0, 500.0f)), VEC3(0, 0, 0), particle_type );
      else
        sim.addParticle(VEC3(seq.between(200.0, 2000.0f), seq.between(100.0, 1000.0f), seq.between(-500.0, 500.0f)), VEC3(0, 0, 0), particle_type);
    }
    updateParticleTypes();
  }

  void drawCell(const CPUSpatialSubdivision::Int3& coords) {
    
    const float inv_world_scale = 1.0f / sim.world_scale;
    VEC3 p0 = sim.spatial_hash.getCellCoords(coords) * inv_world_scale;
    VEC3 p1 = p0 + VEC3::ones * sim.mat.kernel_radius * inv_world_scale;
    TAABB aabb;
    aabb.setMinMax(p0, p1);
    aabb.half *= 0.99f;

    TRandomSequence rsq;
    rsq.setSeed(sim.spatial_hash.gridHash(coords));
    VEC4 color = rsq.between(VEC4(0.1f, 0.2f, 0.1f, 1.0f), Color::White);
    wired_cells.emplace_back(aabb.asMatrix(), color);
  }

  void drawCellId(uint32_t cell_id) {
    const auto& cell_info = sim.spatial_hash.cells_info[cell_id];
    drawCell( cell_info.coords );
  }

  void onRender3D() override {
    TTimer tm;

    float sim_to_world_factor = 1.0f / sim.world_scale;

    sprites.clear();
    wired_cells.clear();
    lines.clear();

    float radius = 1.0f;
    TRandomSequence rsq;

    if (debug_particle >= 0 && debug_particle < sim.num_particles) {
      radius = 0.1f;
      int i = debug_particle;
      const float inv_world_scale = 1.0f / sim.world_scale;

      VEC3 pi = sim.particles_pos.get(i);
      uint32_t cell_id_i = sim.spatial_hash.hashOfCoord(pi);
      CPUSpatialSubdivision::NearRanges near_ranges;
      sim.spatial_hash.collectRanges(near_ranges, cell_id_i);
      TRandomSequence rsq;

      sim.spatial_hash.onEachNeighbourOfParticle(near_ranges, [&](int j) {
        VEC3 pj = sim.particles_pos.get(j);
        //uint32_t cell_id_j = sim.spatial_hash.cells_per_vertex[j].cell_id;
        float dij = (pi - pj).length();
        if ( dij > sim.mat.kernel_radius)
          return;
        int hash_id = sim.spatial_hash.hashOfCoord(pj);
        if (use_cell_colors) {
          rsq.setSeed(hash_id);
          VEC4 color = rsq.between(VEC4(0.1f, 0.2f, 0.1f, 1.0f), Color::White);
          sprites.emplace_back(pj * sim_to_world_factor, 0.5f, i == j ? Color::White : color);
          drawCell(sim.spatial_hash.gridCoords(pj));
        }
        else {
          sprites.emplace_back(pj * sim_to_world_factor, 0.5f, i == j ? Color::DeepPink : Color::Yellow);
        }
        });

      VEC3 vi = sim.particles_vels.get(i) * 10.0;
      lines.addLine(pi * inv_world_scale, ( pi + vi ) * inv_world_scale, Color::White);
    }

    if (show_cells) {
      for (auto& cell : sim.spatial_hash.cells_ranges)
        drawCellId(cell.cell_id);
    }

    {
      PROFILE_SCOPED_NAMED("Sprites");
      for (int i = 0; i < sim.num_particles; ++i) {
        VEC3 p = sim.particles_pos.get(i);
        VEC4 color = colors[sim.particles_type[i]];

        if (use_cell_colors) {
          int hash_id = sim.spatial_hash.hashOfCoord(p);
          rsq.setSeed(hash_id);
          color = rsq.between(VEC4(0.1f, 0.2f, 0.1f, 1.0f), Color::White);
        }

        sprites.emplace_back(p * sim_to_world_factor, radius, color);
      }
    }

    VEC3 interact_point = sim.interact_point * (1.0f / sim.world_scale);
    sprites.emplace_back(interact_point, 1.0f, Color::Magenta);
    lines.addLine(interact_point, interact_point + sim.interact_dir * 0.1f, Color::Magenta);

    {
      PROFILE_SCOPED_NAMED("Flush");
      sprites.drawAll();
      wired_cells.drawAll();
      lines.drawAll();
    }

    {
      PROFILE_SCOPED_NAMED("SDFs");
      sim.sdf.renderWire();
    }
    sim.saveTime(ViscoelasticSim::eSection::Render, tm);
  }
  
  void config3D_32K() {
    sim.mat.rest_density = 3.0f;
    sim.mat.near_stiffness = 1.0f;
    delta_time = 1.0f;
    sim.max_speed = 5.0f;
    sim.mat.kernel_radius = 20.0f;
    gravity_amount = 0.1f;
    gravity_direction = -90.0f;
    num_particles_m0 = 8192;
    num_particles_m1 = 8192;
    num_particles_m2 = 8192;
    sim.using_parallel = true;
    sim.num_particles = 0;
    sim.sdf.prims[3].transform.setPosition(VEC3(0, 0, -5.0));
    sim.sdf.prims[3].transformHasChanged();
    sim.in_2d = false;
    addParticles(32 * 1024);
    updateParticleTypes();
  }

  void renderInMenu() override {

    if (paused) {
      if (ImGui::SmallButton("Run")) {
        auto_pause = false;
        paused = false;
      }
    }

    if (ImGui::SmallButton("Step")) {
      auto_pause = true;
      paused = false;
    }

    if (ImGui::SmallButton("Restart")) {
      int n = sim.num_particles;
      sim.init();
      addParticles(n);
    }

    ImGui::Text("%d Particles / %d Cells", sim.num_particles, (int)sim.spatial_hash.cells_ranges.size());
    ImGui::Checkbox("Using parallel", &sim.using_parallel);
    int max_threads = std::thread::hardware_concurrency();
    int num_threads = sim.num_threads;
    if (ImGui::DragInt("Num Threads", &num_threads, 0.1f, 1, max_threads))
      sim.setNumThreads(num_threads);

    if (ImGui::TreeNode("Simulation Params...")) {
      ImGui::DragFloat("Kernel Radius", &sim.mat.kernel_radius, 0.1f);
      ImGui::DragFloat("Rest Density", &sim.mat.rest_density, 0.1f);
      ImGui::DragFloat("Stiffness", &sim.mat.stiffness, 0.01f, 0.1f, 2.0f);
      ImGui::DragFloat("NearStiffness", &sim.mat.near_stiffness, 0.01f, 0.0f, 2.0f);
      ImGui::DragFloat("Friction", &sim.friction, 0.005f, 0.0f, 1.0f);

      ImGui::DragFloat("Delta Time", &delta_time, 0.005f, 0.0f, 1.0f);
      ImGui::DragFloat("Gravity Direction", &gravity_direction, 1.0f, -360, 360.0f);
      ImGui::DragFloat("Gravity Amount", &gravity_amount, 0.01f, 0.0, 1.0f);
      ImGui::DragFloat("Max Speed", &sim.max_speed, 0.05f, 0, 10.0f);

      ImGui::DragInt("Sub Steps", &sim.num_substeps, 0.02f, 1, 10);

      float buffer_size_mbs = sim.num_particles * sizeof(VEC3) / ( 1024.f * 1024.f );

      ImGui::Text("%1.6lf spatial_hash", sim.times[ ViscoelasticSim::eSection::SpatialHash] );
      ImGui::Text("%1.6lf velocities_update", sim.times[ViscoelasticSim::eSection::VelocitiesUpdate] );
      ImGui::Text("%1.6lf predict_position (BW: %1.0f Mb/s)", sim.times[ViscoelasticSim::eSection::PredictPositions], ( 4.0f * buffer_size_mbs / sim.times[ViscoelasticSim::eSection::PredictPositions]));
      ImGui::Text("%1.6lf relaxation (BW: %1.0f Mb/s)", sim.times[ViscoelasticSim::eSection::Relaxation], (27.0f * 8.0f * 2.0f * buffer_size_mbs / sim.times[ViscoelasticSim::eSection::Relaxation]));
      ImGui::Text("%1.6lf collisions", sim.times[ViscoelasticSim::eSection::Collisions]);
      ImGui::Text("%1.6lf velocities_from_positions", sim.times[ViscoelasticSim::eSection::VelocitiesFromPositions]);
      ImGui::Text("%1.6lf render", sim.times[ViscoelasticSim::eSection::Render]);
      ImGui::Text("%1.6lf Total update (BW: %1.0f Mb/s)", sim.times[ViscoelasticSim::eSection::Update], ( 2.0f * buffer_size_mbs / sim.times[ViscoelasticSim::eSection::Update]));
      ImGui::Text("# Hash Collisions: %d (%1.2f%%)", sim.spatial_hash.num_collisions, (sim.spatial_hash.num_collisions * 100.0 / sim.num_particles) );
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Simulation Debug...")) {
      ImGui::Checkbox("Use Cell Colors", &use_cell_colors);
      ImGui::Checkbox("Show Cells", &show_cells);
      ImGui::Checkbox("Show Particle IDs", &show_ids);
      ImGui::Checkbox("Show Cell IDs", &show_cell_ids);

      if (show_ids) {
        for (int i = 0; i < sim.num_particles; ++i)
          dbg_texts.add(sim.particles_pos.get(i) * (1.0f / sim.world_scale), 0xffffffff, "%d", i);
      }
      if (show_cell_ids) {
        for (auto& cell : sim.spatial_hash.cells_ranges) {
          const auto& cell_info = sim.spatial_hash.cells_info[cell.cell_id];
          VEC3 p = sim.spatial_hash.getCellCoords(cell_info.coords) + VEC3::ones * sim.mat.kernel_radius * 0.5f;
          dbg_texts.add(p * (1.0f / sim.world_scale), 0xffff00ff, "%08x", cell.cell_id);
        }
      }
      dbg_texts.flush();

      if (ImGui::DragInt("Debug Particle", &debug_particle, 0.1f, -1, sim.num_particles))
        sim.debug_particle = debug_particle;
      ImGui::TreePop();
    }

    if (ImGui::SmallButton("Remove All Particles"))
      sim.num_particles = 0;

    if (ImGui::SmallButton("Config 2D 2K Particles")) {
      sim.mat.rest_density = 3.0f;
      sim.mat.near_stiffness = 1.0f;
      sim.friction = 1.0f;
      delta_time = 1.0f;
      sim.max_speed = 5.0f;
      sim.mat.kernel_radius = 20.0f;
      gravity_amount = 0.1f;
      gravity_direction = -90.0f;
      num_particles_m0 = 512;
      num_particles_m1 = 512;
      num_particles_m2 = 512;
      sim.num_particles = 0;
      sim.sdf.prims[3].transform.setPosition(VEC3(0, 0, -5.0));
      sim.sdf.prims[3].transformHasChanged();
      sim.in_2d = true;
      addParticles(2048);
      updateParticleTypes();
    }

    if (ImGui::SmallButton("Config 3D 8K Particles")) {
      sim.mat.rest_density = 3.0f;
      sim.mat.near_stiffness = 1.0f;
      delta_time = 1.0f;
      sim.max_speed = 5.0f;
      sim.mat.kernel_radius = 20.0f;
      gravity_amount = 0.1f;
      gravity_direction = -90.0f;
      num_particles_m0 = 3072;
      num_particles_m1 = 3072;
      num_particles_m2 = 3072;
      sim.num_particles = 0;
      sim.in_2d = false;
      addParticles(8192);
      updateParticleTypes();
    }

    if (ImGui::SmallButton("Config 3D 32K Particles"))
      config3D_32K();

    if (ImGui::TreeNode("Colors..."))
    {
      bool changed = false;
      changed |= ImGui::DragInt("Amount M0", &num_particles_m0, 0.02f, 0, 4096);
      changed |= ImGui::DragInt("Amount M1", &num_particles_m1, 0.02f, 0, 4096);
      changed |= ImGui::DragInt("Amount M2", &num_particles_m2, 0.02f, 0, 4096);
      ImGui::ColorEdit4("Color 0", &colors[0].x);
      ImGui::ColorEdit4("Color 1", &colors[1].x);
      ImGui::ColorEdit4("Color 2", &colors[2].x);
      ImGui::ColorEdit4("Color 3", &colors[3].x);
      changed |= ImGui::DragFloat4("Masses", sim.masses, 0.02f, 0, 4.0f);
      if (changed)
        updateParticleTypes();
      ImGui::TreePop();
    }

    if (debug_particle >= 0 && debug_particle < sim.num_particles) {
      if (ImGui::TreeNode("All Particles around")) {
        int i = debug_particle;
        VEC3 pi = sim.particles_pos.get(i);
        uint32_t cell_id_i = sim.spatial_hash.hashOfCoord(pi);
        ImGui::Text("PARTICLE %d - %08x : %f %f", i, cell_id_i, pi.y, pi.z);
        CPUSpatialSubdivision::NearRanges near_ranges;
        sim.spatial_hash.collectRanges(near_ranges, cell_id_i);
        sim.spatial_hash.onEachNeighbourOfParticle(near_ranges, [&](int j) {
          VEC3 pj = sim.particles_pos.get(j);
          int cell_id_j = sim.spatial_hash.hashOfCoord(pj);
          float dij = (pi - pj).length();
          ImGui::TextColored( dij < sim.mat.kernel_radius ? ImVec4( 0.5f,1.0f,0.5f,1.0f) : ImVec4( 0.5f,0.5f,0.5f,1.0f) 
                            , "Particle %d - %08x : %f %f (%1.1f)", j, cell_id_j, pj.y, pj.z, dij);
          });
        ImGui::TreePop();
      }
    }

    //ImGui::Text("Mouse: %1.1f %1.1f", mouse_cursor.x, mouse_cursor.y);
    //ImGui::Text("Interact Pos: %1.2f %1.2f %1.2f", sim.interact_point.x, sim.interact_point.y, sim.interact_point.z);
    //ImGui::Text("Interact Normal: %1.2f %1.2f %1.2f", sim.interact_dir.x, sim.interact_dir.y, sim.interact_dir.z);
    ImGui::DragFloat("Interact Radius", &sim.interact_rad, 0.1f, 1.0f, 150.0f);

    emitter.renderInMenu();

    if (ImGui::TreeNode("SDFs Config...")) {
      if (ImGui::SmallButton("Box3D Large"))
        sdfLargeCage();
      if (ImGui::SmallButton("Platforms"))
        sdfPlatforms();
      if (ImGui::SmallButton("Inside Box"))
        sdfInsideCage();
      ImGui::Checkbox("Auto rotate first box", &auto_rotate_first_box);
      if( auto_rotate_first_box )
        ImGui::DragFloat( "Rotation Speed", &auto_rotation_speed, 0.01f, -1.0f, 1.0f );
      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Collisions SDF...")) {
      sim.sdf.renderInMenu();
      ImGui::TreePop();
    }

    if (ImGui::Begin("Hints")) {
      ImGui::Text("WSAD  : Move Camera");
      ImGui::Text("W/E/R : Move / Rotate / Scale");
      ImGui::Text("Z Attract");
      ImGui::Text("X Drain");
      ImGui::Text("C Repel");
    }
    ImGui::End();

  }

  void update() override {
    if (!paused) {
      VEC3 gdir = getVectorFromYaw(deg2rad(gravity_direction));
      sim.mat.gravity = VEC3(0, gdir.x, gdir.z) * gravity_amount;
      sim.update(delta_time);
      debug_particle = sim.debug_particle;

      emitter.emit(sim);
    }
    if (auto_pause)
      paused = true;

    if (CCamera* camera = Render::getCurrentRenderCamera()) {
      VEC3 eye = camera->getPosition();
      VEC3 dir = camera->getRayDirectionFromViewportCoords(mouse_cursor.x, mouse_cursor.y);
      VEC3 hit = findNearestIntersectionWithSDF(eye, dir);
      sim.interact_point = hit * sim.world_scale;
      sim.interact_dir = sim.sdf.evalGrad(hit);
    }

    sim.drain = ImGui::IsKeyDown(ImGuiKey::ImGuiKey_X);
    sim.repel = ImGui::IsKeyDown(ImGuiKey::ImGuiKey_C);
    sim.attract = ImGui::IsKeyDown( ImGuiKey::ImGuiKey_Z );

    if (auto_rotate_first_box) {
      for (auto& prim : sim.sdf.prims) {
        if (prim.prim_type == SDF::Primitive::eType::BOX) {
          prim.transform.setRotation(QUAT::createFromAxisAngle(VEC3::axis_x, deg2rad(auto_rotation_speed)) * prim.transform.getRotation());
          prim.transformHasChanged();
          break;
        }
      }
    }


  }

  VEC3 findNearestIntersectionWithSDF(VEC3 ray_src, VEC3 ray_dir) const {
    
    float best_t = FLT_MAX;
    ray_src += ray_dir * 0.01f;
    for (auto& p : sim.sdf.planes) {
      float num = -( p.d + ray_src.dot(p.n) );
      float den = ray_dir.dot(p.n);
      // Skip planes not looking at the camera
      if (den >= 0.0)
        continue;
      float t = num / den;
      if (t < best_t)
        best_t = t;
    }

    // test vs the other primitives...
    // ..

    return ray_src + best_t * ray_dir;
  }

};

ViscoelasticModule module_viscoelastic;