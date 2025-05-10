#ifndef INC_RENDER_PLATFORM_DX11_H_
#define INC_RENDER_PLATFORM_DX11_H_

#ifndef PLATFORM_WINDOWS
#error  PLATFORM should be windows
#endif

// Forward reference's of pointer for DX
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Texture2D;
struct ID3D11Texture3D;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11InputLayout;

namespace RenderPlatform {

  extern ID3D11Device* device;
  extern ID3D11DeviceContext* ctx;

  ENGINE_API int width();
  ENGINE_API int height();

  ENGINE_API ID3D11DeviceContext* getContext();

  bool create(HWND hWnd);
  void destroy();
  bool resizeBackBuffer(int xres, int yres);
  void swapFrames();
  void beginFrame();

  enum ePrimitiveType {
    ePoint = 1, //D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
    eLines = 2, //D3D_PRIMITIVE_TOPOLOGY_LINELIST,
    eTriangles = 4, //D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    eTrianglesStrip = 5, //D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
  };

  enum eVertexType {
    VTX_TYPE_POS,
    VTX_TYPE_POS_COLOR,
    VTX_TYPE_POS_UV,
    VTX_TYPE_POS_NORMAL_UV,
    VTX_TYPE_POS_NORMAL_UV_POS2,
    VTX_TYPE_POS_NORMAL_UV_COLOR_ATTR,
  };

  struct TCmdBuffer {
  };

  struct ENGINE_API TMesh {
    ID3D11Buffer* vb = nullptr;
    ID3D11Buffer* ib = nullptr;
    ID3D11ShaderResourceView*  srv = nullptr;
    ID3D11UnorderedAccessView* uav = nullptr;
    ID3D11UnorderedAccessView* uav_ib = nullptr;
    std::vector< uint8_t >     cpu_vertices;
    std::vector< uint8_t >     cpu_indices;

    bool createIndexBuffer(uint32_t new_nindices, uint32_t new_bytes_per_index, const void* index_data, bool create_uav);
    void addRef();
  };

  struct TVertexDecl {
    void*  elems = nullptr;
    size_t num_elems = 0;
    //bool create(const json& j);
    void destroy();
    uint32_t expectedSize() const;
  };

  struct ENGINE_API TShaderCte {
    std::vector< uint8_t >    cpu_vertices;
    ID3D11Buffer* cb = nullptr;
    uint32_t                  slot = 0;
    bool                      is_dirty = false;
    bool createPlatform(uint32_t nbytes);
    void destroy();
    bool isValid() const { return cb != nullptr; }
  };

  struct TTexture {
    ID3D11Texture2D* resource = nullptr;
    ID3D11Texture3D* resource3d = nullptr;
    ID3D11ShaderResourceView* resource_view = nullptr;
    ID3D11UnorderedAccessView* uav = nullptr;

    // Values match the DX values, and are static_asserted in the .cpp
    enum eFormat {
      FMT_BGRA_8UNORM = 87 // DXGI_FORMAT_B8G8R8A8_UNORM
    , FMT_RGBA_8UNORM = 28 //DXGI_FORMAT_R8G8B8A8_UNORM
    , FMT_PVRTC_RGBA_2BPP
    , FMT_PVRTC_RGBA_4BPP
    , FMT_DEPTH_32F = 41 //DXGI_FORMAT_R32_FLOAT
    , FMT_R_8UNORM = 61 //DXGI_FORMAT_R8_UNORM
    , FMT_RGBA_32F = 2 //DXGI_FORMAT_R32G32B32A32_FLOAT
    , FMT_R_32F = 41 //DXGI_FORMAT_R32_FLOAT
    , FMT_R_16F = 54 //DXGI_FORMAT_R16_FLOAT
    , FMT_INVALID = 0 //DXGI_FORMAT_UNKNOWN
    };

    bool createPlatform(
      uint32_t new_xres, uint32_t new_yres,
      eFormat new_pixel_format,
      int new_nmipmaps,
      bool is_cubemap,
      bool is_render_target,
      bool generate_uav,
      bool is_dynamic
    );
    bool create3DPlatform(
      uint32_t new_xres, uint32_t new_yres, uint32_t new_zres,
      eFormat new_pixel_format,
      int  new_nmipmaps,
      bool is_dynamic,
      bool is_render_target,
      bool generate_uav
    );

    bool create2DPlatform( uint32_t width, uint32_t height, const void* data, eFormat fmt );

    void destroyPlatform();

    void* asVoidPtrID() {
      return resource_view;
    }
  };

  // -----------------------------------------
  struct TRenderToTexture {
    ID3D11RenderTargetView* render_target_view = nullptr;
    ID3D11DepthStencilView* depth_stencil_view = nullptr;
    ID3D11ShaderResourceView* depth_srv = nullptr;
    void activateRT();
    void destroyRT();
    void clearRenderTarget(VEC4 color);
    void clearDepth(float clearDepthValue = 1.0f);
  };

  // -----------------------------------------
  struct ENGINE_API TGPUBuffer {
    ID3D11Buffer* buffer = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11UnorderedAccessView* uav = nullptr;
    ID3D11Buffer* buffer_clean = nullptr;
    TBuffer                      cpu_data;

    bool copyGPUtoCPU(void* out_addr = nullptr) const;

    template< typename TData >
    const TData* getData() const {
      if (!copyGPUtoCPU())
        return nullptr;
      return (const TData*)cpu_data.data();
    }

    bool isValid() const {
      return buffer != nullptr;
    }

    void addRef();
  };

  // -----------------------------------------
  class ENGINE_API TPipelineState {

  public:

    class CShaderBase {
    public:
      std::string shader_src;
      std::string shader_fn;
      std::string shader_profile;

      struct CShaderReflectionInfo;
      CShaderReflectionInfo* reflection_info = nullptr;

      bool scanResourcesFrom(const TBuffer& Blob);
    };

    // -----------------------------------------
    class CVertexShader : public CShaderBase {
      ID3D11VertexShader* vs = nullptr;
      ID3D11InputLayout*  vtx_layout = nullptr;
      std::string         shader_vtx_type_name;
    public:
      void destroy();
      bool compile(
        const char* szFileName
        , const char* szEntryPoint
        , const char* profile
        , const char* vertex_type_name
      );
      void activate() const;
      bool isValid() const { return vs != nullptr; }
    };

    // -----------------------------------------
    class CPixelShader : public CShaderBase {
      ID3D11PixelShader* ps = nullptr;
    public:
      void destroy();
      bool compile(
        const char* szFileName
        , const char* szEntryPoint
        , const char* profile
      );
      void activate() const;
      bool isValid() const { return ps != nullptr; }
    };

    // -----------------------------------------
    class ENGINE_API CComputeShader : public CShaderBase {
      ID3D11ComputeShader* cs = nullptr;
    public:
      void destroy();

      bool compile(
        const char* szFileName
        , const char* szEntryPoint
        , const char* profile
        , std::string* error_msg_returned = nullptr
      );
      void activate() const;
      void deactivate() const;
      bool isValid() const { return cs != nullptr; }
    };

  public:
    CVertexShader         vs;
    CPixelShader          ps;
    CComputeShader        cs;
    int                   blend_cfg = 0;
  };

  // -----------------------------------------
  struct TRenderPass {
    int         sample_count = 1;
  };

  // -----------------------------------------
  struct TComputeTask {
  };

  // -----------------------------------------
  void imGuiInit();
  void imGuiBegin();
  void imGuiShutdown();

}

#include "render/gpu_trace.h"
#define GPU_SCOPED(name) CGpuScope gpu_scope(name); PROFILE_SCOPED_NAMED(name)

#endif /* Header_h */
