#include "platform.h"
#include "module.h"
#include "module_render.h"
#include "render/render.h"
//#include "render/primitives.h"
//#include "render/manager.h"
#include "render/color.h"
#include "formats/mesh/io_mesh.h"
#include "render/font.h"

using namespace Render;

extern OrthoLight light;
extern FogParams fog;

bool createMeshFromFilename(Render::Mesh* mesh, const char* name) {
  TBuffer ibuf;
  if (!ibuf.load(name))
    return false;
  IOMesh::RawMesh rm;
  if (!IOMesh::load(ibuf, rm))
    return false;
  const VertexDecl* vdecl = getVertexDeclByName(rm.header.vertex_decl_name);
  assert(vdecl->bytes_per_vertex == rm.header.bytes_per_vertex);
  if (!mesh->create(
    rm.vertices.data(),
    rm.header.nvertices,
    (Render::ePrimitiveType)rm.header.primitive_type,
    rm.header.nindices ? rm.indices.data() : nullptr,
    rm.header.nindices,
    rm.header.bytes_per_index,
    vdecl
  ))
    return false;
  mesh->aabb = TAABB(rm.header.aabb_center, rm.header.aabb_half);
  mesh->ranges = rm.ranges;
  return true;
}

IResource* createMesh(const char* name) {
  TStr64 sname(name);
  Render::Mesh* mesh = new Render::Mesh();
  if (sname == "grid.mesh")       createGrid(mesh);
  else if (sname == "axis.mesh")  createAxis(mesh);
  else if (sname == "unit_wired_cube.mesh")  createUnitWiredCube(mesh);
  else if (sname == "unit_circle_xz.mesh")  createUnitWiredCircleXZ(mesh, 32);
  else if (sname == "line.mesh") createLine(mesh);
  else if (sname == "view_volume.mesh") createViewVolume(mesh);
  else if (sname == "quad_xy_basic.mesh") createQuadXYBasic(mesh);
  else if (sname == "quad_xz.mesh") createQuadXZ(mesh);
  else if (sname == "quad_xy_01.mesh") createQuadXY(mesh, 1.0f, VEC2(0.5f, 0.5f));
  else if (sname == "quad_xy_01_textured.mesh") createQuadXYTextured(mesh, 1.0f, 2, VEC2(0.5f, 0.5f));
  else if (sname == "cube.mesh") createUnitCube(mesh);
  else {
    if (!createMeshFromFilename(mesh, name)) {
      delete mesh;
      return nullptr;
    }
  }
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

IResource* createTexture(const char* name) {
  TBuffer ibuf;
  if (!ibuf.load(name))
    return nullptr;
  qoi_desc desc = { 0 };
  void* data = qoi_decode(ibuf.data(), (int)ibuf.size(), &desc, 4);
  Render::Texture* obj = new Render::Texture();
  bool is_ok = obj->create(desc.width, desc.height, data, eFormat::RGBA_8UNORM, 1);
  free(data);
  if (is_ok) {
    obj->setName(name);
  }
  else {
    delete obj;
    return nullptr;
  }
  return obj;
}

template< typename T>
const T* readResource(const json& j, const char* attr_name, const char* default_value = nullptr) {
  const char* res_name = j.value(attr_name, default_value);
  if (!res_name) return nullptr;
  return Resource<T>(res_name);
}

// Read a const Texture* from a json as a string
template<>
void load(json j, Render::Texture const*& texture) {
  if (!j.is_string())
    texture = nullptr;
  else
    texture = Resource<Render::Texture>(j);
}

template<>
void load(json j, Render::Buffer const*& buffer) {
  buffer = Resource<Render::Buffer>(j);
}

struct ModuleResources : public Module {
  int getPriority() const override { return 20; }
  const char* getName() const override { return "resources"; }
  bool startsLoaded() const override { return true; }

  const char* input_config = "data/input.json";
  const char* light_config = "data/light.json";
  const char* fog_config = "data/fog.json";

  struct Scene {
    TStr64   name;
    uint32_t first = 0;
    uint32_t count = 0;
    void destroy() {
      freeCteObjects(first, count);
      count = 0;
    }
    void create(const char* iname) {
      assert(count == 0);

      JsonFile jscene(iname);
      json jobjs = jscene;

      name.from(iname);
      count = (uint32_t)jobjs.size();
      first = allocCteObjects(count);
      dbg("Using %d slots from pos %d for scene %s\n", count, first, iname);

      onEachArr(jobjs, [this](json j, size_t idx) {
        uint32_t obj_idx = first + (uint32_t)idx;

        const Render::Mesh* mesh = readResource<Render::Mesh>(j, "mesh");
        if (!mesh) return;

        const Render::PipelineState* pso = readResource<Render::PipelineState>(j, "pso");
        uint32_t submesh = j.value("submesh", 0);
        Render::Manager::get().add(mesh, pso, obj_idx, (uint8_t)submesh);

        CteObject& obj = *getCteObjectsData(obj_idx);

        TTransform t;
        t.load(j);
        obj.world = t.asMatrix();
        obj.color = j.value("color", Colors::White);
        });
    }
  };
  Vector< Scene > scenes;

  void loadScene(const char* iname) {
    Scene s;
    s.create(iname);
    scenes.push_back(s);
  }

  void load() override {
    addResourcesFactory("mesh", &createMesh);
    addResourcesFactory("font", &createFont);
    addResourcesFactory("qoi", &createTexture);
    addResourcesFactory("texture", &createPlatformTexture);

    Input::get().load(JsonFile(input_config));
    light.load(JsonFile(light_config));
    fog.load(JsonFile(fog_config));

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

  }

  void onSceneChanged(AppMsgOnFileChanged& msg) {
    auto s = std::find_if(scenes.begin(), scenes.end(), [&](const Scene& s) {
      return s.name == msg.filename;
      });
    if (s == scenes.end())
      return;
    s->destroy();
    s->create(msg.filename);
  }

};

ModuleResources module_resources;
