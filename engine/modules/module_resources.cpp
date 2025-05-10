#include "platform.h"
#include "module.h"
#include "module_render.h"
#include "render/render.h"
//#include "render/primitives.h"
//#include "render/manager.h"
#include "render/color.h"
//#include "formats/mesh/io_mesh.h"

using namespace Render;

IResource* createMesh(const char* name) {
  Render::Mesh* mesh = new Render::Mesh();
  //if (sname == "grid.mesh")       createGrid(mesh);
  //else if (sname == "axis.mesh")  createAxis(mesh);
  //else if (sname == "unit_wired_cube.mesh")  createUnitWiredCube(mesh);
  //else if (sname == "unit_circle_xz.mesh")  createUnitWiredCircleXZ(mesh, 32);
  //else if (sname == "line.mesh") createLine(mesh);
  //else if (sname == "view_volume.mesh") createViewVolume(mesh);
  //else if (sname == "quad_xy_basic.mesh") createQuadXYBasic(mesh);
  //else if (sname == "quad_xz.mesh") createQuadXZ(mesh);
  //else if (sname == "quad_xy_01.mesh") createQuadXY(mesh, 1.0f, VEC2(0.5f, 0.5f));
  //else if (sname == "quad_xy_01_textured.mesh") createQuadXYTextured(mesh, 1.0f, 2, VEC2(0.5f, 0.5f));
  //else if (sname == "cube.mesh") createUnitCube(mesh);
  //else
  //  return nullptr;
  mesh->setName(name);
  return mesh;
}

IResource* createPlatformTexture(const char* name) {
  TBuffer ibuf;
  if (!ibuf.load(name))
    return nullptr;
  Render::Texture* obj = new Render::Texture();
  if (!obj->decodeFrom(ibuf)) {
    delete obj;
    return nullptr;
  }
  obj->setName(name);
  return obj;
}

//template< typename T>
//const T* readResource(const json& j, const char* attr_name, const char* default_value = nullptr) {
//  const char* res_name = j.value(attr_name, default_value);
//  if (!res_name) return nullptr;
//  return Resource<T>(res_name);
//}

//template<>
//void load(json j, Render::Buffer const*& buffer) {
//  buffer = Resource<Render::Buffer>(j);
//}

struct ModuleResources : public Module {
  int getPriority() const override { return 20; }
  const char* getName() const override { return "resources"; }
  bool startsLoaded() const override { return true; }

  void load() override {
    /*
    // Pipelines are created from this file
    JsonFile jfile("data/resources.json");
    json jpso = jfile;
    onEachArr(jpso, [](json j, size_t idx) {
      TStr32 jtype(j.value("type", ""));
      if (jtype.empty())
        return;
      TStr32 in_name(j.value("name", ""));

      TStr64 res_name("%s.%s", in_name.c_str(), jtype.c_str());
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
      else if (jtype == "plugin") {
        emit(AppMsgActionPlugin{ in_name, AppMsgActionPlugin::eLoad });
        return;

      }
      else if (jtype == "plugins") {
        onEachArr(j["names"], [&](const char* in_name, size_t idx) {
          emit(AppMsgActionPlugin{ in_name, AppMsgActionPlugin::eLoad });
          });
        return;
      }
      else if (jtype == "buffer") {
        Render::Buffer* buffer = new Render::Buffer;
        buffer->create(j);
        res = buffer;

      }
      else {
        fatal("Invalid resource type %s\n", jtype.c_str());
        return;
      }

      res->setName(res_name.c_str());
      addResource(res);
      });
*/
  }

};

ModuleResources module_resources;
