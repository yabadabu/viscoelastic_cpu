#pragma once

#include "render/color.h"
#include "dx11/render_platform.h"

class CCamera;

namespace Render {

  //struct ENGINE_API TMesh : public RenderPlatform::TMesh, public IResource {
  struct ENGINE_API TMesh : public IResource {
  };

  // -------------------------------------------------------
  //public RenderPlatform::Buffer, 
  struct ENGINE_API Buffer : public IResource {
    int                slot = 0;
    uint32_t           bytes_per_instance = 0;
    uint32_t           num_instances = 1;
    bool create(size_t nbytes);
    void destroy() override;
    void setName(const char* new_name) override;
    void* rawData();
    size_t size() const { return num_instances * bytes_per_instance; }
  };

  template< typename T >
  struct ENGINE_API TypedBuffer : public Buffer {
    bool create(int in_slot, const char* label, uint32_t in_num_instances = 1) {
      slot = in_slot;
      num_instances = in_num_instances;
      bytes_per_instance = sizeof(T);
      if (!Buffer::create(sizeof(T) * in_num_instances))
        return false;
      setName(label);
      return true;
    }
    T* data() {
      return static_cast<T*>(rawData());
    }
    T* data(uint32_t instance_id) {
      assert(instance_id < num_instances);
      return data() + instance_id;
    }
  };

  struct TPipelineState;

  void drawLine(VEC3 src, VEC3 target, VEC4 color);
  void drawInstancedPrimitives(const TMesh* mesh, const void* data, uint32_t ninstances, const TPipelineState* pipe = nullptr);
  void drawPrimitive(const TMesh* mesh, MAT44 world, VEC4 color);
  CCamera* getCurrentRenderCamera();

  template< typename InstanceData >
  class InstancesData : public std::vector< InstanceData > {
  protected:
    const Render::TMesh* mesh = nullptr;
    const char* mesh_name = nullptr;
  public:
    InstancesData() = default;
    InstancesData(const char* new_mesh_name) : mesh_name(new_mesh_name) {
      assert(mesh_name);
    }
    void drawAll() {
      if (!mesh) {
        assert(mesh_name);
        mesh = Resource<Render::TMesh>(mesh_name);
      }
      assert(mesh);
      Render::drawInstancedPrimitives(mesh, std::vector< InstanceData >::data(), (uint32_t)std::vector< InstanceData >::size());
    }
  };

  // To render in a single batch
  struct Instance {
    MAT44 world;
    VEC4  color;
    Instance() {}
    Instance(MAT44 nworld, VEC4 ncolor) : world(nworld), color(ncolor) {}
  };

  struct SpriteInstance {
    VEC3  pos;
    float scale = 1.0f;
    VEC4  color;
    SpriteInstance() = default;
    SpriteInstance(VEC3 apos, float ascale, VEC4 acolor) : pos(apos), scale(ascale), color(acolor) {}
  };

  class ENGINE_API VInstances : public InstancesData< Instance > {
  public:
    VInstances() = default;
    VInstances(const char* mesh_name) : InstancesData< Instance >(mesh_name) {}
    void addLine(VEC3 src, VEC3 dst, VEC4 color);
  };

  class VSpriteInstances : public InstancesData< SpriteInstance > {
  public:
    void drawAll();
  };
}


/*

#include "vertex_types.h"
#include "resources/resource.h"
//#include "handle/handle.h"
#include "render/color.h"
#include "raw_mesh.h"

class CCamera;
struct TCtesObject;

namespace Render {

  // -------------------------------------------------------
  enum class ePrimitiveType {
    ePoint = RenderPlatform::ePoint,
    eLines = RenderPlatform::eLines,
    eTriangles = RenderPlatform::eTriangles,
    eTrianglesStrip = RenderPlatform::eTrianglesStrip,
    eInvalidPrimitiveType
  };

  // -------------------------------------------------------
  enum class eDepthState {
    LESS = 0,
    DISABLE_ALL,
    TEST_BUT_NO_WRITE,
    SHADOW_MAP,
    TEST_EQUAL,
    TEST_INVERTED,
    COUNT,
    DEFAULT = LESS,
  };
  //extern NamedValues<eDepthState> depthStates;

  // -------------------------------------------------------
  enum class eSamplerState {
    LINEAR_WRAP = 0,
    LINEAR_CLAMP,
    POINT_CLAMP,
    PCF_SHADOWS,
    COUNT,
    DEFAULT = LINEAR_WRAP,
  };

  // -------------------------------------------------------
  enum class eRSConfig {
    DEFAULT = 0,
    REVERSE_CULLING,
    CULL_NONE,
    SHADOWS,
    WIREFRAME,
    COUNT
  };
  extern NamedValues<eRSConfig> rsConfigs;

  // ---------------------------------------
  enum class eBlendConfig {
    DEFAULT,
    ADDITIVE,
    ADDITIVE_BY_SRC_ALPHA,
    COMBINATIVE,
    COUNT
  };
  extern NamedValues<eBlendConfig> blendConfigs;

#define COLOR_ARGB(a,r,g,b) (((a)<<24)|((r)<<16)|((g)<<8)|((b)))

  // -------------------------------------------------------
  struct TVertexDecl : public RenderPlatform::TVertexDecl, public IResource {
    uint32_t                        bytes_per_vertex = 0;
    uint32_t                        getBytesPerVertex() const { return bytes_per_vertex; }
    bool create(const json& j);
    bool isCompatibleWith(const TVertexDecl* other) const;
    static const TVertexDecl* getByName(const char* name);
    static void registerNew(const TVertexDecl* new_vdecl);
  };

  // -------------------------------------------------------
  template< typename TVertex >
  ENGINE_API const TVertexDecl* getVertexDecl();

  // -------------------------------------------------------
  struct TGPUBuffer;

  // -------------------------------------------------------
  struct ENGINE_API TMesh : public RenderPlatform::TMesh, public IResource {
    const TVertexDecl* vertex_decl = nullptr;
    uint32_t           nvertices = 0;
    uint32_t           nindices = 0;
    uint32_t           bytes_per_index = 0;
    uint32_t           ninstances = 1;
    TRawMesh::VGroups  groups;
    TAABB              aabb;

    ePrimitiveType     primitive_type = ePrimitiveType::eInvalidPrimitiveType;

    void setName(const std::string& new_name) override;

    template< typename TVertex >
    bool create(ePrimitiveType new_prim_type, const TVertex* vertex_data, uint32_t nvertexs) {
      return create(new_prim_type, vertex_data, nvertexs, getVertexDecl<TVertex>());
    }

    template< typename TVertex >
    bool create(ePrimitiveType new_prim_type, const std::vector< TVertex >& vertexs) {
      return create(new_prim_type, vertexs.data(), (uint32_t)vertexs.size(), getVertexDecl<TVertex>());
    }

    bool create(const json& j) override;

    static constexpr uint32_t FLAG_DYNAMIC = 1;
    static constexpr uint32_t FLAG_ACCESS_FROM_COMPUTE = 2;
    static constexpr uint32_t FLAG_ACCESS_FOR_IDXS_FROM_COMPUTE = 4;

    bool create(
      ePrimitiveType new_prim_type
      , const void* vertex_data
      , uint32_t nvertexs
      , const TVertexDecl* vertex_decl
      , const void* index_data = nullptr
      , uint32_t nindices = 0
      , uint32_t bytes_per_index = 2
      , uint32_t flags = 0
    );

    void debugInMenu() override;
    void destroy() override;
    bool isValid() const override;

  };

  // -------------------------------------------------------
  struct ENGINE_API TShaderCteBase : public RenderPlatform::TShaderCte {
    bool createBase(uint32_t slot, uint32_t nbytes, const char* label);
    bool createBaseArray(uint32_t slot, uint32_t nbytes_per_instance, uint32_t ninstances, const char* label);
    void uploadToGPUBase(const void* data, uint32_t data_bytes, uint32_t dst_offset = 0);
  };

  // -------------------------------------------------------
  struct TShaderCteSingleBase : public TShaderCteBase {};

  template< typename TData >
  struct TShaderCte : public TData, public TShaderCteSingleBase {
    bool create(int new_slot, const char* label) {
      return createBase(new_slot, sizeof(TData), label);
    }
    void uploadToGPU() {
      uploadToGPUBase((const TData*)this, sizeof(TData));
    }
    void set(const TData& new_value) { *(TData*)this = new_value; }
  };

  // -------------------------------------------------------
  struct TShaderCteArrayBase : public TShaderCteBase {
    uint32_t ninstances;
    uint32_t bytes_per_instance;
  };

  template< typename TData >
  struct TShaderCtesArray : public TShaderCteArrayBase {
    bool create(uint32_t new_slot, uint32_t new_ninstances, const char* label) {
      assert(new_slot >= 0);
      assert(label);
      assert(new_ninstances > 0);
      ninstances = new_ninstances;
      bytes_per_instance = sizeof(TData);
      return createBaseArray(new_slot, sizeof(TData), ninstances, label);
    }
    void set(uint32_t idx, const TData& data) {
      assert(idx < ninstances);
      uploadToGPUBase(&data, sizeof(TData), idx * sizeof(TData));
    }
    //void setRange( uint32_t start_idx, const TData* first_data, uint32_t ndata_instances ) {
    //  assert( start_idx < ninstances );
    //  assert( start_idx + ndata_instances <= ninstances );
    //  assert( first_data );
    //  assert( ndata_instances > 0 );
    //  uploadToGPUBase( first_data, sizeof( TData ) * ndata_instances, start_idx * sizeof( TData ) );
    //}
  };


  // -------------------------------------------------------
  struct ENGINE_API TPipelineState : public RenderPlatform::TPipelineState, public IResource {

    struct TTextureSlot {
      const TTexture* texture = nullptr;
      int             slot = 0;
    };
    std::vector< TTextureSlot > textures;

    struct TGPUBufferSlot {
      const TGPUBuffer* buffer = nullptr;
      int               slot = 0;
    };
    std::vector< TGPUBufferSlot > gpu_buffers;

    const TVertexDecl* vert_decl = nullptr;
    eRSConfig          rs_cfg = eRSConfig::DEFAULT;

    bool uses_textures = true;

    bool create(const json& json) override;
    bool createForCompute(const json& json);
    void parseGPUBuffers(const json& jcfg);
    void parseTextures(const json& jcfg);
    void onFileChanged(const std::string& filename) override;
    void debugInMenu() override;
    void destroy() override;
    void eachUnassignedResource(std::function<void(const char*, uint32_t idx)> resolver) const;
  };

  // -------------------------------------------------------
  struct TRenderPass : public RenderPlatform::TRenderPass {
    enum eFlags {
      RP_USE_BACKBUFFER_COLOR = BIT(0),
      RP_IS_SHADOW_MAP = BIT(1)
    };

    struct TColorAttach {
      VEC4               clear_color = VEC4(0, 0, 0, 0);
      TRenderToTexture* rt = nullptr;
      bool               enabled = false;
      TTexture::eFormat  format = TTexture::eFormat::BGRA_8UNORM;
      uint32_t           flags = 0;
    };

    struct TDepthAttach {
      float              clear_value = 1.f;
      TRenderToTexture* rt = nullptr;
      bool               enabled = false;
      TTexture::eFormat  format = TTexture::eFormat::DEPTH_32F;
      uint32_t           flags = 0;
    };

    struct TAction {
      enum eAction {
        ACTIVATE_TEXTURE,
        ACTIVATE_OUTPUT_OF_PASS,
        RENDER_CATEGORY,
        SET_BLENDING,
        SET_DEPTH_STATE,
        SET_RASTERIZER,
        SEND_MSG,
        RENDER_DEBUG_INTERFACE,
        RENDER_DEBUG_3D,
        RUN_COMPUTE_SHADERS,
        RENDER_SHADOW_MAPS,
        ACTIVATE_LIGHTS,
        INVALID_ACTION
      };
      eAction            action = INVALID_ACTION;
      bool               enabled = true;
      union {
        struct {
          const TTexture* texture;
          int                slot;
        } activate_texture;
        struct {
          uint32_t           name_id;
          int                slot;
        } activate_output_of_pass;
        struct {
          const char* cat_name;
          uint32_t           name_id;
        } render_category;
        struct {
          const char* str;
          uint32_t           str_id;
        } text;
        eBlendConfig         blend_config;
        eDepthState          depth_state;
        eRSConfig            rasterizer_config;
      };
    };
    std::vector< TAction > actions;

    static const int max_color_attachments = 4;
    TStr64                     name;
    TStrID                     name_id;
    TColorAttach               colors[max_color_attachments];
    TDepthAttach               depth;
    uint32_t                   cfg_width = 0, cfg_height = 0;
    uint32_t                   width = 0, height = 0;       // Auto
    CHandle                    light_dir;     // Component, not entity
    bool                       sets_render_targets = true;
    bool                       is_compute = false;

    bool create();
    void destroy();

    static TRenderPass* create(const json& j);
    void debugInMenu();
    void runComputeTasks();
    void beginPass();
    void execute();
    void endPass();
    bool usesBackBuffer() const;

    static void setCurrentRenderCamera(CHandle h_camera);
    static void activateCamera(CCamera* c, const TViewport& vp );
  };

  // -------------------------------------------------------
  struct ENGINE_API TGPUBuffer : public RenderPlatform::TGPUBuffer, public IResource {

    enum class eDataType {
      NONE, FLOAT, FLOAT3, FLOAT4, COLOR, INT, INT2, INT4, MATRIX, OTHERS, CTES_STRUCT, DRAW_INDIRECT_INSTANCING_ARGS, BOOL, COUNT
    };
    static size_t bytesPerDataType(enum eDataType new_data_type);

    struct TMember {
      TStr16    name;
      uint32_t  offset = 0;
      eDataType data_type = eDataType::NONE;
      // For imgui
      float     vmin = 0.f;
      float     vmax = 2.f;
      float     vspeed = 0.01f;
    };

    eDataType data_type = eDataType::NONE;
    TStrID    data_type_id = 0;
    uint32_t  num_elems = 0;
    uint32_t  bytes_per_elem = 0;
    uint32_t  total_bytes = 0;
    bool      autocleared = false;
    bool      append = false;
    bool      has_counter = false;
    bool      is_raw = false;
    bool      is_draw_indirect = false;

    std::vector< TMember > members;
    TShaderCteSingleBase   cte;

    bool createCte(int slot, const TBuffer& initial_data);
    bool create(const TBuffer& initial_data);
    bool create(const json& j) override;
    void clear(u8 new_value = 0);
    void debugInMenu() override;
    void destroy() override;
    bool createSimple(uint32_t new_bytes_per_elem, uint32_t new_num_elems, bool is_indirect, bool is_raw);
    bool configure(uint32_t new_num_elems, Render::TGPUBuffer::eDataType new_data_type, const char* name, bool new_is_raw = false);
    void copyCPUtoGPUFrom(const void* new_data, size_t num_bytes) const;
    void copyFromGPU(const TGPUBuffer& other);
    bool isValid() const override {
      return RenderPlatform::TGPUBuffer::isValid();
    }

    // Update the members by name
    bool setMember(const char* member_name, const void* new_data, uint32_t new_bytes);
    template< typename TData >
    bool setMember(const char* member_name, const TData& data) {
      return setMember(member_name, &data, sizeof(TData));
    }
    void setName(const std::string& new_name) override;
  };

  // -------------------------------------------------------
  struct ENGINE_API TComputeTask : public RenderPlatform::TComputeTask {
    TStr64                     name;
    TStrID                     name_id;
    const TPipelineState* pipeline = nullptr;
    int                        sizes[3] = { 1,1,1 };
    int                        groups[3] = { 4,4,1 };
    bool                       enabled = true;
    std::vector< const TTexture* >   textures;
    std::vector< const TGPUBuffer* > buffers;
    std::vector< const TMesh* >      meshes;
    void runTask() const;
    void runTaskWithGroups(int gx, int gy, int gz) const;
    void runTaskIndirect(const TGPUBuffer& buffer_with_args, uint32_t offset = 0) const;
    void activateResources();
    static void beginTasks();
    static void endTasks();
    static std::vector< TGPUBuffer* > auto_cleared_buffers;
    bool create(const json& j);
    bool activateCte(const char* name, const TShaderCteBase& cte) const;
    bool activateBuffer(const char* name, const TGPUBuffer& buf) const;
    bool activateMesh(const char* name, const TMesh& mesh) const;
    bool activateTexture(const char* name, const TTexture& texture) const;
    bool activateMeshIndex(const char* name, const TMesh& mesh) const;
    void setGroupSizes(int gx, int gy, int gz = 1) {
      groups[0] = gx;
      groups[1] = gy;
      groups[2] = gz;
    }
  };

  // -------------------------------------------------------
  struct ENGINE_API TCmdBuffer : public RenderPlatform::TCmdBuffer {

    static const TPipelineState* curr_pipeline;

    void draw(const TMesh& mesh, int group_idx = 0);
    void drawInstanced(const TMesh& mesh, uint32_t num_instances, int group_idx = 0);
    void drawInstanced(const ePrimitiveType prim_type, uint32_t vertexs_per_instance, uint32_t ninstances);
    void drawInstancedIndirect(const TMesh& mesh, const TGPUBuffer& buffer, uint32_t offset);
    void drawInstancedIndirect(const ePrimitiveType prim_type, const TGPUBuffer& buffer, uint32_t offset);
    void updateCte(const TShaderCteSingleBase& cte, const void* data);
    void updateBuffer(const TGPUBuffer& buffer, const void* data);
    void activateCte(const TShaderCteBase& cte);
    void activateCteIndexed(const TShaderCteArrayBase& cte, uint32_t idx);
    const TPipelineState* setPipelineState(const TPipelineState& pipeline);
    void setViewport(const TViewport& vp);
    void setDepthState(eDepthState depth_state);
    void setSamplerState(eSamplerState sampler_state);
    void setRasterizerState(eRSConfig rs_config);

    // Imgui
    void beginImGui();
    void renderImGuiDrawData(ImDrawData* data);

    void setFragmentTexture(const TTexture& texture, int slot);
    void activateMaterial(const TMaterial& material);
    void activateMesh(const TMesh& mesh, int slot = -1);
    void activateBuffer(const char* name, const TGPUBuffer& buffer);
    void pushGroup(const char* label);
    void popGroup();
    void deactivateMesh(int slot = 0);
    void deactivateTexture(int slot = 0);
  };

  // -------------------------------------------------------
  extern std::vector< TRenderPass* > render_passes;
  extern std::vector< TComputeTask* > compute_tasks;
  extern std::vector< TMaterialTypeSlot > material_type_slots;
  void debugInMenu();

  // -------------------------------------------------------
  ENGINE_API void drawAxis(MAT44 world);
  ENGINE_API void drawViewVolume(const CCamera& camera);
  ENGINE_API void drawAABB(const TAABB aabb, MAT44 ext_world, VEC4 color, bool only_corners = false);

  ENGINE_API void activateObj(const MAT44 world, VEC4 color);
  ENGINE_API void activateDebugPipeline(const TMesh* mesh);

  ENGINE_API bool getClientAreaViewport(TViewport& vp);
}

#include "primitives.h"

*/