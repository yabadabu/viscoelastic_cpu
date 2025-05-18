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
    float                    strength = 10.0f;
    int                      rate = 1;
    bool                     enabled = false;
    int                      particle_type = 0;
    bool renderInMenu() {
      if (!ImGui::TreeNode("Emitter"))
        return false;
      bool changed = transform.debugInMenu();
      ImGui::Checkbox("Emitting", &enabled);
      ImGui::DragFloat("Radius", &radius, 0.01f, 0.1f, 3.0f);
      ImGui::DragFloat("Strength", &strength, 0.1f, 1.0f, 25.0f);
      ImGui::DragInt("Rate", &rate, 0.1f, 0, 32);
      ImGui::DragInt("Type", &particle_type, 0.1f, 0, 3);
      ImGui::TreePop();
      return changed;
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
  CDebugTexts dbg_texts;

  int       debug_particle = -1;

  ViscoelasticModule() {
    dbg("ViscoelasticModule::ViscoelasticModule\n");
    sim.init();
    sim.in_2d = true;

    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3::zero, VEC3::axis_y));
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(0, 6, 0), -VEC3::axis_y));
    const float sz = 2.5f;
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(0, 0, sz), -VEC3::axis_z));
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(0, 0, 0), VEC3::axis_z));
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(sz, 0, 0), -VEC3::axis_x));
    sim.sdf.prims.push_back(SDF::Primitive::makePlane(VEC3(0, 0, 0), VEC3::axis_x));

    sim.sdf.prims.push_back(SDF::Primitive::makeBox(VEC3(0, 1.5, 1.0), VEC3(1.0f, 2.0f, 3.0f) * 0.1f ));

    emitter.transform.setPosition(VEC3(0.0f, 3.0f, -2.0f));

    addParticles(512);
  }

  void load() {
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

  void onRender3D() {
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

    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
    if (ImGui::TreeNode("Simulation Params...")) {
      ImGui::DragFloat("Kernel Radius", &sim.mat.kernel_radius, 0.1f);
      ImGui::DragFloat("Rest Density", &sim.mat.rest_density, 0.1f);
      ImGui::DragFloat("Stiffness", &sim.mat.stiffness, 0.01f, 0.1f, 2.0f);
      ImGui::DragFloat("NearStiffness", &sim.mat.near_stiffness, 0.01f, 0.0f, 2.0f);
      ImGui::DragFloat("Friction", &sim.friction, 0.005f, 0.0f, 1.0f);

      ImGui::DragFloat("Delta Time", &delta_time, 0.005f, 0.0f, 1.0f);
      ImGui::DragFloat("Gravity Direction", &gravity_direction, 1.0f, -360, 360.0f);
      ImGui::DragFloat("Gravity Amount", &gravity_amount, 0.01f, 0.0, 1.0f);
      ImGui::DragFloat("Max Speed", &sim.max_speed, 0.05f, 0, 5.0f);

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

    if (ImGui::SmallButton("Add 100 particles..."))
      addParticles(100);

    if (ImGui::SmallButton("Add 1024 particles..."))
      addParticles(1024);

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

    if (ImGui::SmallButton("Config 3D 32K Particles")) {
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

    ImGui::Text("Interact: %1.2f %1.2f %1.2f", sim.interact_point.x, sim.interact_point.y, sim.interact_point.z);
    ImGui::Text("Interact Dir: %1.2f %1.2f %1.2f", interact_dir.x, interact_dir.y, interact_dir.z);
    ImGui::DragFloat("Interact Radius", &sim.interact_rad, 0.1f, 1.0f, 150.0f);

    emitter.renderInMenu();
    if (ImGui::TreeNode("Collisions SDF...")) {
      sim.sdf.renderInMenu();
      ImGui::TreePop();
    }

    if (ImGui::Begin("Hints")) {
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

      if (emitter.enabled) {
        static TRandomSequence rseq;
        for (int i = 0; i < emitter.rate; ++i)
        {
          VEC3 p = emitter.transform.transformCoord(VEC3(rseq.between( -emitter.radius, emitter.radius ), rseq.between(-emitter.radius, emitter.radius), 0.0f)) * sim.world_scale;
          VEC3 v = emitter.transform.transformDir(VEC3(0.0f, 0.0f, 1.0f)) * emitter.strength;
          sim.addParticle(p, v, emitter.particle_type);
        }
      }

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
  }

  VEC3 findNearestIntersectionWithSDF(VEC3 src, VEC3 dir) const {
    float best_t = FLT_MAX;
    for (auto& p : sim.sdf.planes) {
      float num = -( p.d + src.dot(p.n) );
      float den = dir.dot(p.n);
      // Skip planes not looking at the camera
      if (den >= 0.0)
        continue;
      float t = num / den;
      if (t < best_t)
        best_t = t;
    }
    return src + best_t * dir;
  }

};

ViscoelasticModule module_viscoelastic;