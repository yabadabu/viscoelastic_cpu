#include "platform.h"
#include "module.h"
#include "module_render.h"
#include "render/render.h"
#include "render/primitives.h"
#include "render/color.h"

using namespace Render;

IResource* createMesh(const std::string& name) {
  Render::Mesh* mesh = new Render::Mesh();
  if (name == "grid.mesh")       createGrid(mesh);
  //else if (sname == "axis.mesh")  createAxis(mesh);
  else if (name == "unit_wired_cube.mesh")  createUnitWiredCube(mesh);
  else if (name == "unit_circle_xz.mesh")  createUnitWiredCircleXZ(mesh, 32);
  else if (name == "line.mesh") createLine(mesh);
  else if (name == "unit_quad_xy.mesh") createUnitQuadXY(mesh);
  //else if (sname == "view_volume.mesh") createViewVolume(mesh);
  //else if (sname == "quad_xy_basic.mesh") createQuadXYBasic(mesh);
  //else if (sname == "quad_xz.mesh") createQuadXZ(mesh);
  //else if (sname == "quad_xy_01.mesh") createQuadXY(mesh, 1.0f, VEC2(0.5f, 0.5f));
  //else if (sname == "quad_xy_01_textured.mesh") createQuadXYTextured(mesh, 1.0f, 2, VEC2(0.5f, 0.5f));
  //else if (sname == "cube.mesh") createUnitCube(mesh);
  else
    return nullptr;
  mesh->setName(name.c_str());
  return mesh;
}

template<>
void load(json j, Render::Buffer const*& buffer) {
  buffer = Resource<Render::Buffer>(j);
}

struct ModuleResources : public Module {
  int getPriority() const override { return 20; }
  const char* getName() const override { return "resources"; }

  void load() override {
   
    // Pipelines are created from this file
    JsonFile jfile("data/resources.json");
    json jpso = jfile;
    onEachArr(jpso, [](json j, size_t idx) {
      std::string jtype(j.value("type", ""));
      if (jtype.empty())
        return;
      std::string in_name(j.value("name", ""));

      std::string res_name = in_name + "." + jtype;
      IResource* res = nullptr;

      if (jtype == "pso") {
        Render::PipelineState* pso = new Render::PipelineState;
        pso->create(j);
        res = pso;
      }
      else if (jtype == "buffer") {
        Render::Buffer* buffer = new Render::Buffer;
        buffer->create(j);
        res = buffer;
      }
      else if (jtype == "mesh") {
        res = createMesh(res_name);

      }
      else {
        fatal("Invalid resource type %s\n", jtype.c_str());
        return;
      }

      res->setName(res_name.c_str());
      addResource(res);
      });

  }

  void unload() override {
    destroyAllResources();
  }

};

ModuleResources module_resources;
