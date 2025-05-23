#include "platform.h"
#include "render/render.h"
#include "render/vertex_types.h"

namespace Render {

  // -------------------------------------------------------
  //NamedValues<eDepthState> depthStates = {
  //   { eDepthState::LESS, "less" },
  //   { eDepthState::DISABLE_ALL, "disable_all" },
  //   { eDepthState::TEST_BUT_NO_WRITE, "test_but_no_write" },
  //   { eDepthState::SHADOW_MAP, "shadow_map" },
  //   { eDepthState::TEST_EQUAL, "test_equal" },
  //   { eDepthState::TEST_INVERTED, "test_inverted" },
  //   { eDepthState::LESS, "default" },
  //};

  //// -------------------------------------------------------
  //NamedValues<eRSConfig> rsConfigs = {
  //   { eRSConfig::CULL_NONE, "cull_none" },
  //   //{ eRSConfig::REVERSE_CULLING, "cull_reverse" },
  //   //{ eRSConfig::WIREFRAME, "wireframe" },
  //   { eRSConfig::SHADOWS, "shadows" },
  //   { eRSConfig::DEFAULT, "default" },
  //};

  //// -------------------------------------------------------
  //NamedValues<eBlendConfig> blendConfigs = {
  //  { eBlendConfig::DEFAULT, "default" },
  //  { eBlendConfig::ADDITIVE, "additive" },
  //  { eBlendConfig::ADDITIVE_BY_SRC_ALPHA, "additive_premultiplied" },
  //  { eBlendConfig::COMBINATIVE, "combinative" },
  //};

  //// -------------------------------------------------------
  //NamedValues<eFormat> formatNames = {
  //{ eFormat::R8_UNORM, "R8_UNORM" },
  //{ eFormat::R16_UNORM, "R16_UNORM" },
  //{ eFormat::R32_FLOAT, "R32_FLOAT" },
  //};

  // -------------------------------------------------------
  static Encoder* main_encoder = nullptr;

  void setMainEncoder(Encoder* in_encoder) {
    main_encoder = in_encoder;
  }

  Encoder* getMainEncoder() {
    return main_encoder;
  }

  // -------------------------------------------------------
  bool PipelineState::loadCommonConfig(const json& j) {
    //const char* rs_cfg_str = j.value("rs", "default");
    //rs_cfg = rsConfigs.valueOf(rs_cfg_str);
    //if (rs_cfg == eRSConfig::COUNT)
    //  fatal("Invalid rs config %s\n", rs_cfg_str);

    //const char* blend_cfg_str = j.value("blending", "default");
    //blend_cfg = blendConfigs.valueOf(blend_cfg_str);
    //if (blend_cfg == eBlendConfig::COUNT)
    //  fatal("Invalid blend config %s\n", blend_cfg_str);

    //const char* depth_cfg_str = j.value("depth", "default");
    //depth_state = depthStates.valueOf(depth_cfg_str);
    //if (depth_state == eDepthState::COUNT)
    //  fatal("Invalid depth state config %s\n", depth_cfg_str);

    //textures = j.value("textures", textures);
    buffers = j.value("buffers", buffers);

    //TStr32 category_name(j.value("category", "solid"));
    //if (category_name == "solid")
    //  category = CategorySolids;
    //else if (category_name == "shadows")
    //  category = CategoryShadowCasters;
    //else if (category_name == "debug3d")
    //  category = CategoryDebug3D;
    //else
    //  fatal("Invalid category %s\n", category_name.c_str());

    return true;
  }

  // -------------------------------------------------------
  VertexDecl vdecl_pos(sizeof(TVtxPos), "Pos");
  VertexDecl vdecl_pos_color(sizeof(TVtxPosColor), "PosColor");
  VertexDecl vdecl_pos_normal(sizeof(TVtxPosN), "PosN");
  VertexDecl vdecl_pos_normal_uv(sizeof(TVtxPosNUv), "PosNUv");

  template<>
  const VertexDecl* getVertexDecl<Render::TVtxPos>() { return &vdecl_pos; }
  template<>
  const VertexDecl* getVertexDecl<Render::TVtxPosColor>() { return &vdecl_pos_color; }
  template<>
  const VertexDecl* getVertexDecl<Render::TVtxPosN>() { return &vdecl_pos_normal; }
  template<>
  const VertexDecl* getVertexDecl<Render::TVtxPosNUv>() { return &vdecl_pos_normal_uv; }

  const VertexDecl* all_decls[] = {
    &vdecl_pos,
    &vdecl_pos_color,
    &vdecl_pos_normal,
    &vdecl_pos_normal_uv,
    nullptr,
  };

  const VertexDecl* getVertexDeclByName(const char* name) {
    const VertexDecl** v = all_decls;
    while (*v) {
      if (strcmp((*v)->name, name) == 0)
        return *v;
      ++v;
    }
    fatal("Failed to find vertex declaration with name %s\n", name);
    return nullptr;
  }

  bool Buffer::create(const json& j) {
    const char* in_name = j.value("name", "");
    slot = j.value("slot", 0);
    bytes_per_instance = j.value("bytes_per_instance", 0);
    num_instances = j.value("num_instances", 1);
    int total_bytes = bytes_per_instance * num_instances;
    if (!create(total_bytes))
      return false;
    setName(in_name);
    return true;
  }


  void drawInstancedPrimitivesWithGPUBuffer(const Mesh* mesh, const Instance* data, uint32_t ninstances, const Render::PipelineState* pso, uint32_t submesh, Buffer* gpu_instances) {

    if (!data || !ninstances)
      return;

    if (!mesh)
      fatal("drawInstancedPrimitives invalid mesh\n");

    Encoder* encoder = getMainEncoder();
    assert(encoder);

    if (!pso)
      pso = Resource<PipelineState>("basic.pso");

    assert(pso);
    assert(mesh);
    assert(gpu_instances);
    assert(pso->vertex_decl == mesh->vertex_decl);
    encoder->setRenderPipelineState(pso);

    const u8* bytes = (const u8*)data;
    uint32_t remaining = ninstances;
    while (remaining > 0) {
      uint32_t block = std::min(remaining, gpu_instances->num_instances);
      uint32_t block_size_bytes = block * gpu_instances->bytes_per_instance;
      encoder->setBufferContents(gpu_instances, bytes, block_size_bytes);
      encoder->drawInstancedMesh(mesh, block, submesh);
      remaining -= block;
      bytes += block_size_bytes;
    }

  }

  void drawPrimitive(const Mesh* mesh, const MAT44& world, VEC4 color, const PipelineState* pso, uint32_t submesh) {
    Buffer* gpu_instances = (Buffer*)Resource<Buffer>("single_instance.buffer");
    assert(gpu_instances);
    Instance instance = { world, color };
    drawInstancedPrimitivesWithGPUBuffer(mesh, &instance, 1, pso, submesh, gpu_instances);
  }

  void drawInstancedPrimitives(const Mesh* mesh, const Instance* data, uint32_t ninstances, const Render::PipelineState* pso, uint32_t submesh) {
    Buffer* gpu_instances = (Buffer*)Resource<Buffer>("instances.buffer");
    assert(gpu_instances);
    drawInstancedPrimitivesWithGPUBuffer(mesh, data, ninstances, pso, submesh, gpu_instances);
  }

  void VInstances::addLine(VEC3 src, VEC3 dst, VEC4 color) {
    emplace_back(MAT44::createLine(src, dst), color);
  }

  void VInstances::add(const TAABB& aabb, VEC4 color) {
    emplace_back(aabb.asMatrix(), color);
  }

  void VInstances::add(const TAABB& aabb, const MAT44& transform, VEC4 color) {
    emplace_back(aabb.asMatrix() * transform, color);
  }

  void VSpriteInstances::drawAll() {
    if (!mesh)
      mesh = Resource<Mesh>("unit_quad_xy.mesh");
    if (!pso)
      pso = Resource<PipelineState>("sprites.pso");
    assert(pso && mesh && pso->buffers.size() == 1);
    uint32_t ninstances = (uint32_t)size();
    Buffer* gpu_instances = (Buffer*)pso->buffers[0];

    Encoder* encoder = getMainEncoder();
    assert(encoder);
    encoder->setRenderPipelineState(pso);

    const u8* bytes = (const u8*)data();
    uint32_t remaining = ninstances;
    while (remaining > 0) {
      uint32_t block = std::min(remaining, gpu_instances->num_instances);
      uint32_t block_size_bytes = block * gpu_instances->bytes_per_instance;
      encoder->setBufferContents(gpu_instances, bytes, block_size_bytes);
      encoder->drawInstancedMesh(mesh, block, 0);
      remaining -= block;
      bytes += block_size_bytes;
    }

  }

}
