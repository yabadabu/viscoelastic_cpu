#pragma once

#include "dx11/render_platform.h"
#include "resources/resource.h"
#include "vertex_declarations.h"

namespace Render {
	
  // -------------------------------------------------------
  // This needs to be platform independent. I choose the enums
  // from metal and will do the translation to platform in side the 
  // render_platform.h
  enum ePrimitiveType {
    eLines = 1,
    eTriangles = 3,
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

  // -------------------------------------------------------
  enum class eSamplerState {
	  LINEAR_WRAP = 0,
	  LINEAR_CLAMP,
	  PCF_SHADOWS,
	  //POINT_CLAMP,
	  COUNT,
	  DEFAULT = LINEAR_WRAP,
  };

  // -------------------------------------------------------
  enum class eRSConfig {
	  DEFAULT = 0,
	  //REVERSE_CULLING,
	  CULL_NONE,
	  SHADOWS,
	  //WIREFRAME,
	  COUNT
  };

  // ---------------------------------------
  enum class eBlendConfig {
	  DEFAULT,
	  ADDITIVE,
	  ADDITIVE_BY_SRC_ALPHA,
	  COMBINATIVE,
	  COUNT
  };

  enum class eFormat {
	  INVALID = 0
	, BGRA_8UNORM
	, RGBA_8UNORM
	, R8_UNORM
  , R16_UNORM
	, R32_FLOAT
  };

  // -------------------------------------------------------
  using Category = uint32_t;
  static constexpr Category CategorySolids(0x100);
  static constexpr Category CategoryShadowCasters(0x110);
  static constexpr Category CategoryPostFX(0x120);
  static constexpr Category CategoryDebug3D(0x130);
  static constexpr Category CategoryUI(0x140);

  // -------------------------------------------------------
  struct VertexDecl : public RenderPlatform::VertexDecl {
    uint32_t    bytes_per_vertex = 0;
    const char* name = nullptr;
    VertexDecl() = default;
    VertexDecl(size_t new_bytes_per_vertex, const char* new_name) : bytes_per_vertex((uint32_t)new_bytes_per_vertex), name(new_name) {}
  };

  // -------------------------------------------------------
  struct Mesh : public RenderPlatform::Mesh, public IResource {
    uint32_t           nvertices = 0;
    uint32_t           nindices = 0;
    ePrimitiveType     primitive_type = ePrimitiveType::eInvalidPrimitiveType;
    uint32_t           bytes_per_index = 0;
    const VertexDecl*  vertex_decl = nullptr;
    TAABB              aabb;

    struct Range {
      uint32_t first, count;
    };
    std::vector< Range > ranges;

    bool create( 
    	const void* in_data, 
    	uint32_t in_nvertices, 
    	ePrimitiveType in_primitive_type, 
      const void* in_index_data,
      uint32_t in_nindices,
      uint32_t bytes_per_index,
      const VertexDecl* in_vertex_decl
    );

    template< typename T, typename IndexType = uint16_t>
    bool create( 
      const T* in_data, 
      uint32_t in_nvertices, 
      ePrimitiveType in_primitive_type,
      const IndexType* in_index_data = nullptr,
      uint32_t in_nindices = 0
    ) {
      return create( in_data, 
                     in_nvertices, 
                     in_primitive_type, 
                     in_index_data, 
                     in_nindices, 
                     sizeof(IndexType), 
                     getVertexDecl<T>());
    }
    void destroy() override;
    void setName( const char* new_name ) override;
  };

  // -------------------------------------------------------
  struct Texture : public RenderPlatform::Texture, public IResource {
    bool create(uint32_t width, uint32_t height, const void* raw_data, eFormat fmt, int num_mips);
    bool decodeFrom(const TBuffer& buffer);
    void destroy() override;
    void setName(const char* new_name) override;
  };

  // -------------------------------------------------------
  struct RenderToTexture : public Texture, public RenderPlatform::RenderToTexture {
      // We are owners of this texture
      Texture*                  depth_texture = nullptr;
      int                       width = 0;
      int                       height = 0;
      void beginRendering();
      void endRendering();

      bool createRT(const char* name, int new_xres, int new_yres, eFormat new_clr, eFormat new_z);
      void destroy() override;
  };

  // -------------------------------------------------------
  struct Buffer : public RenderPlatform::Buffer, public IResource {
    int                slot = 0;
    uint32_t           bytes_per_instance = 0;
    uint32_t           num_instances = 1;
    bool create( size_t nbytes );
    bool create(const json& j);
    void destroy() override;
    void setName( const char* new_name ) override;
    void* rawData();
    size_t size() const { return num_instances * bytes_per_instance; }
  };

  template< typename T > 
  struct TypedBuffer : public Buffer {
    bool create( int in_slot, const char* label, uint32_t in_num_instances = 1) {
      slot = in_slot;
      num_instances = in_num_instances;
      bytes_per_instance = sizeof( T );
      if( !Buffer::create( sizeof( T ) * in_num_instances ) )
        return false;
      setName( label );
      return true;
	}
	T* data() {
		return static_cast<T*>(rawData());
	}
	T* data( uint32_t instance_id ) {
    assert( instance_id < num_instances);
		return data() + instance_id;
	}
  };

  // -------------------------------------------------------
  struct PipelineState : public RenderPlatform::PipelineState, public IResource {
    const VertexDecl* vertex_decl = nullptr;
    int                  priority = 0;
    uint32_t             category = CategorySolids;
    eDepthState          depth_state = eDepthState::DEFAULT;
    eRSConfig            rs_cfg = eRSConfig::DEFAULT;
    eBlendConfig         blend_cfg = eBlendConfig::DEFAULT;
    const PipelineState* shadows_pso = nullptr;

    std::vector<const Texture*> textures;
    std::vector<const Buffer*> buffers;

    bool create(const json& j);
    void setName(const char* new_name) override;
    void destroy() override;
    bool reloadShaders();

  private:
    bool loadCommonConfig(const json& j);

  };

  // -------------------------------------------------------
  struct Encoder : public RenderPlatform::Encoder {
    void drawMesh( const Mesh* mesh, uint32_t submesh = 0 );
    void drawInstancedMesh( const Mesh* mesh, int ninstances, uint32_t submesh = 0 );
    void setRenderPipelineState( const PipelineState* in_pipeline );
    void setVertexBuffer( const Buffer* in_buffer );
    void setVertexBufferOffset(int instance_idx, int bytes_per_instance, int slot);
    void setTexture(const Texture* texture, int slot);
    void pushGroup( const char* label );
    void popGroup( );
    void setBufferContents( Buffer* buffer, const void* new_data, size_t total_bytes );
  };
  
  void setMainEncoder( Encoder* in_encoder );
  Encoder* getMainEncoder( );
  
  // -------------------------------------------------------
  // To render in a single batch
  struct Instance {
    MAT44 world;
    VEC4  color;
    int   mat_id = 0;
    VEC3  dummy;
    Instance() {}
    Instance(MAT44 nworld, VEC4 ncolor) : world(nworld), color(ncolor), mat_id(0) {}
    Instance(MAT44 nworld, VEC4 ncolor, int nmat_id) : world(nworld), color(ncolor), mat_id(nmat_id) {}
  };

  // -------------------------------------------------------
  void drawInstancedPrimitives(const Mesh* mesh, const Instance* data, uint32_t ninstances, const PipelineState* pso = nullptr, uint32_t submesh = 0);
  void drawPrimitive(const Mesh* mesh, const MAT44& world, VEC4 color = VEC4(1,1,1,1), const PipelineState* pso = nullptr, uint32_t submesh = 0);
  void allocInstancedPrimitives(const Instance* instances_data, uint32_t ninstances);
  void frameStarts();

  template< typename InstanceData >
  class InstancesData : public std::vector< InstanceData > {
  protected:
    const Mesh* mesh = nullptr;
    const PipelineState* pso = nullptr;
    const char* mesh_name = nullptr;
    const char* pso_name = nullptr;
  public:
    InstancesData() = default;
    InstancesData(const char* in_mesh_name, const char* in_pso_name) 
    : mesh_name( in_mesh_name )
    , pso_name( in_pso_name )
    {
      assert(mesh_name);
      mesh = Resource<Mesh>(mesh_name);
      if( pso_name )
        pso = Resource<PipelineState>(pso_name);
      assert(mesh);
    }
    void drawAll( const PipelineState* in_pso = nullptr ) const {
      drawInstancedPrimitives(mesh, std::vector< InstanceData >::data(), (uint32_t)std::vector< InstanceData >::size(), in_pso ? in_pso : pso );
    }
    const Mesh* getMesh() const { return mesh; }
    void drawAsShadowCasters( ) const {
      if( pso && pso->shadows_pso )
        drawAll( pso->shadows_pso );
    }
  };

  class VInstances : public InstancesData< Instance > {
  public:
    VInstances() = default;
    VInstances(const char* in_mesh_name, const char* in_pso_name = nullptr) : InstancesData< Instance >( in_mesh_name, in_pso_name ) {} ;
    void addLine(VEC3 src, VEC3 dst, VEC4 color);
    void add(const TAABB& aabb, VEC4 color);
    void add(const TAABB& aabb, const MAT44& transform, VEC4 color);
  };

  // ---------------------------------------------------------------------
  struct SpriteInstance {
    VEC3  pos;
    float scale = 1.0f;
    VEC4  color;
    SpriteInstance() = default;
    SpriteInstance(VEC3 apos, float ascale, VEC4 acolor) : pos(apos), scale(ascale), color(acolor) {}
  };

  class VSpriteInstances : public InstancesData< SpriteInstance > {
  public:
    void drawAll();
  };

  // ---------------------------------------------------------------------
  void drawCameraVolume(const CCamera& camera, VEC4 color);
  CCamera* getCurrentRenderCamera();
}


#include "color.h"

