#include "platform.h"
#include "render/render.h"
#include "render/debug_texts.h"
#include "viscoelastic_sim.h"

struct ViscoelasticModule : public Module {

  const char* getName() const override { return "viscoelastic"; }

  ViscoelasticSim          sim;
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
    addParticles(512);
  }

  void load() {
    wired_cells = Render::VInstances("unit_wired_cube.mesh");
    lines = Render::VInstances( "line.mesh" );
  }

  void addParticles(int num) {
    TRandomSequence seq(54123);
    for (int i = 0; i < num; ++i) {
      if (sim.in_2d)
        sim.addParticle(VEC3(0.0f, seq.between(200.0, 1000.0f), seq.between(-500.0, 500.0f)), VEC3(0, 0, 0));
      else
        sim.addParticle(VEC3(seq.between(200.0, 2000.0f), seq.between(100.0, 1000.0f), seq.between(-500.0, 500.0f)), VEC3(0, 0, 0));
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

  void onRenderDebug3D() {
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

    sprites.drawAll();
    wired_cells.drawAll();
    lines.drawAll();

    sim.sdf.renderWire();
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

    ImGui::DragFloat("Grid Radius", &sim.mat.kernel_radius, 0.1f);
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
    ImGui::Text("%1.6lf relaxation (BW: %1.0f Mb/s)", sim.times[ViscoelasticSim::eSection::Relaxation], (20.0f * 2.0f * buffer_size_mbs / sim.times[ViscoelasticSim::eSection::Relaxation]));
    ImGui::Text("%1.6lf collisions", sim.times[ViscoelasticSim::eSection::Collisions]);
    ImGui::Text("%1.6lf velocities_from_positions", sim.times[ViscoelasticSim::eSection::VelocitiesFromPositions]);
    ImGui::Text("%1.6lf render", sim.times[ViscoelasticSim::eSection::Render]);
    ImGui::Text("%1.6lf Total update (BW: %1.0f Mb/s)", sim.times[ViscoelasticSim::eSection::Update], ( 2.0f * buffer_size_mbs / sim.times[ViscoelasticSim::eSection::Update]));

    ImGui::Text("%d Particles / %d Cells", sim.num_particles, (int)sim.spatial_hash.cells_ranges.size());
    ImGui::Text("# Hash Collisions: %d (%1.2f%%)", sim.spatial_hash.num_collisions, (sim.spatial_hash.num_collisions * 100.0 / sim.num_particles) );
    ImGui::Checkbox("Use Cell Colors", &use_cell_colors);
    ImGui::Checkbox("Show Cells", &show_cells);
    ImGui::Checkbox("Show Particle IDs", &show_ids);
    ImGui::Checkbox("Show Cell IDs", &show_cell_ids);
    ImGui::Checkbox("Using parallel", &sim.using_parallel);

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

    if (ImGui::SmallButton("Add 100 particles..."))
      addParticles(100);
    if (ImGui::SmallButton("Add 1024 particles..."))
      addParticles(1024);

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
      sim.num_particles = 0;
      sim.sdf.prims[3].transform.setPosition(VEC3(0, 0, -5.0));
      sim.sdf.prims[3].transformHasChanged();
      sim.in_2d = false;
      addParticles(32 * 1024);
      updateParticleTypes();
    }

    bool changed = false;
    changed |= ImGui::DragInt("Amount M0", &num_particles_m0, 0.02f, 0, 4096);
    changed |= ImGui::DragInt("Amount M1", &num_particles_m1, 0.02f, 0, 4096);
    changed |= ImGui::DragInt("Amount M2", &num_particles_m2, 0.02f, 0, 4096);
    ImGui::ColorEdit4("Color 0", &colors[0].x);
    ImGui::ColorEdit4("Color 1", &colors[1].x);
    ImGui::ColorEdit4("Color 2", &colors[2].x);
    ImGui::ColorEdit4("Color 3", &colors[3].x);
    changed |= ImGui::DragFloat4("Masses", sim.masses, 0.02f, 0, 4.0f);
    if (ImGui::DragInt("Debug Particle", &debug_particle, 0.1f, -1, sim.num_particles))
      sim.debug_particle = debug_particle;

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

    if (changed)
      updateParticleTypes();

    sim.sdf.renderInMenu();
  }

  void update() override {
    if (!paused) {
      VEC3 gdir = getVectorFromYaw(deg2rad(gravity_direction));
      sim.mat.gravity = VEC3(0, gdir.x, gdir.z) * gravity_amount;
      sim.update(delta_time);
      debug_particle = sim.debug_particle;
    }
    if (auto_pause)
      paused = true;

    if (CCamera* camera = Render::getCurrentRenderCamera()) {
      VEC2 mouse; //= ImGui::getInput().mouse_pixels;
      VEC3 dir = camera->getRayDirectionFromViewportCoords(mouse.x, mouse.y);
      float t_hit = -camera->getPosition().x / dir.x;
      VEC3 hit = camera->getPosition() + t_hit * dir;
      sim.mouse_prev = sim.mouse;
      sim.mouse = hit * sim.world_scale;
    }

    //sim.drain = getInput().key('C').isPressed();
    //sim.repel = getInput().key('R').isPressed();
    //sim.attract = getInput().key('Z').isPressed();
  }

};

DECLARE_MODULE(ViscoelasticModule);