#include "platform.h"
#include "render/render.h"
#include "formats/pvr/pvr_loader.h"
#include "resources/resources_manager.h"
#include <d3dcompiler.h>
#include <d3dcompiler.inl>
#include "DDSTextureLoader.h"
#include "formats/tga/tga_loader.h"
#include "render/gpu_trace.h"
#include "utils/cache_manager.h"
#include "formats/png/lodepng.h"

// ImGui 
#include "application.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "render/dx11/render_platform.h"
IMGUI_IMPL_API LRESULT  ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3d9.lib")

#if defined(_DEBUG) || defined(PROFILE)
#define setDbgName(obj,name) (obj)->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT) strlen( name ), name )
#else
#define setDbgName(obj,name) do { } while(false)
#endif

static_assert(RenderPlatform::ePrimitiveType::ePoint == D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
static_assert(RenderPlatform::ePrimitiveType::eLines == D3D_PRIMITIVE_TOPOLOGY_LINELIST);
static_assert(RenderPlatform::ePrimitiveType::eTriangles == D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
static_assert(RenderPlatform::ePrimitiveType::eTrianglesStrip == D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

static_assert(RenderPlatform::TTexture::eFormat::FMT_BGRA_8UNORM == DXGI_FORMAT_B8G8R8A8_UNORM);
static_assert(RenderPlatform::TTexture::eFormat::FMT_RGBA_8UNORM == DXGI_FORMAT_R8G8B8A8_UNORM);
static_assert(RenderPlatform::TTexture::eFormat::FMT_DEPTH_32F == DXGI_FORMAT_R32_FLOAT);
static_assert(RenderPlatform::TTexture::eFormat::FMT_R_8UNORM == DXGI_FORMAT_R8_UNORM);
static_assert(RenderPlatform::TTexture::eFormat::FMT_R_32F == DXGI_FORMAT_R32_FLOAT);
static_assert(RenderPlatform::TTexture::eFormat::FMT_R_16F == DXGI_FORMAT_R16_FLOAT);
static_assert(RenderPlatform::TTexture::eFormat::FMT_INVALID == DXGI_FORMAT_UNKNOWN);

#define SAFE_RELEASE(x) if( x ) x->Release(), x = nullptr

// ---------------------------------------------------------------
namespace RenderPlatform {

  ID3D11Device* device = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  IDXGISwapChain* swap_chain = nullptr;
  Render::TRenderToTexture* rt_backbuffer = nullptr;
  int                     xres, yres;

  ID3D11DepthStencilState* z_cfgs[(int)Render::eDepthState::COUNT];
  ID3D11SamplerState* all_samplers[(int)Render::eSamplerState::COUNT];
  ID3D11BlendState* blend_states[(int)Render::eBlendConfig::COUNT];
  ID3D11RasterizerState* rasterize_states[(int)Render::eRSConfig::COUNT];

  ID3D11DeviceContext* getContext() { return ctx; }
  int width() { return xres; }
  int height() { return yres; }

  // Define details of private struct
  struct TPipelineState::CShaderBase::CShaderReflectionInfo {
    std::vector<D3D11_SHADER_INPUT_BIND_DESC> bound_resources;
    const D3D11_SHADER_INPUT_BIND_DESC* findBindPoint(const char* name) const;
  };

  // ----------------------------------------------------------------------
  bool createSamplers() {
    D3D11_SAMPLER_DESC sampDesc;

    auto createState = [&](Render::eSamplerState cfg, const D3D11_SAMPLER_DESC& desc, const char* title) {
        HRESULT hr = device->CreateSamplerState(&desc, &all_samplers[(int)cfg]);
        if (FAILED(hr))
            return false;
        setDbgName(all_samplers[(int)cfg], title);
        return true;
    };

    {
      // Create the sample state
      ZeroMemory(&sampDesc, sizeof(sampDesc));
      sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
      sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
      sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
      sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      sampDesc.MinLOD = 0;
      sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
      if (!createState(Render::eSamplerState::LINEAR_WRAP, sampDesc, "WRAP_LINEAR"))
          return false;
    }

    {
      ZeroMemory(&sampDesc, sizeof(sampDesc));
      sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      sampDesc.MinLOD = 0;
      sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
      if (!createState(Render::eSamplerState::LINEAR_CLAMP, sampDesc, "CLAMP_LINEAR"))
          return false;
    }

    {
      ZeroMemory(&sampDesc, sizeof(sampDesc));
      sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
      sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
      sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
      sampDesc.MinLOD = 0;
      sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
      if (!createState(Render::eSamplerState::POINT_CLAMP, sampDesc, "CLAMP_POINT"))
          return false;
    }

    {
      // PCF sampling
      D3D11_SAMPLER_DESC desc = {
        D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,// D3D11_FILTER Filter;
        D3D11_TEXTURE_ADDRESS_BORDER, //D3D11_TEXTURE_ADDRESS_MODE AddressU;
        D3D11_TEXTURE_ADDRESS_BORDER, //D3D11_TEXTURE_ADDRESS_MODE AddressV;
        D3D11_TEXTURE_ADDRESS_BORDER, //D3D11_TEXTURE_ADDRESS_MODE AddressW;
        0,//FLOAT MipLODBias;
        0,//UINT MaxAnisotropy;
        D3D11_COMPARISON_LESS, //D3D11_COMPARISON_FUNC ComparisonFunc;
        0.0, 0.0, 0.0, 0.0,//FLOAT BorderColor[ 4 ];
        0,//FLOAT MinLOD;
        0//FLOAT MaxLOD;   
      };
      if (!createState(Render::eSamplerState::PCF_SHADOWS, sampDesc, "PCF_SHADOWS"))
          return false;
    }

    return true;
  }

  void destroySamplers() {
    for (auto c : all_samplers)
      SAFE_RELEASE(c);
  }

  // ----------------------------------------------------------------
  // Create Depth states
  bool createDepthStates() {
    D3D11_DEPTH_STENCIL_DESC desc;

    auto createState = [&](Render::eDepthState cfg, const D3D11_DEPTH_STENCIL_DESC& desc, const char* title) {
        HRESULT hr = device->CreateDepthStencilState(&desc, &z_cfgs[(int)cfg]);
        if (FAILED(hr))
            return false;
        setDbgName(z_cfgs[(int)cfg], title);
        return true;
    };

    memset(&desc, 0x00, sizeof(desc));
    desc.DepthEnable = FALSE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.StencilEnable = FALSE;
    if (!createState(Render::eDepthState::DISABLE_ALL, desc, "DISABLE_ALL"))
        return false;

    // Default app, only pass those which are near than the previous samples
    memset(&desc, 0x00, sizeof(desc));
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D11_COMPARISON_LESS;
    desc.StencilEnable = FALSE;
    if (!createState(Render::eDepthState::DEFAULT, desc, "ZCFG_DEFAULT"))
        return false;

    // test but don't write. Used while rendering particles for example
    memset(&desc, 0x00, sizeof(desc));
    desc.DepthEnable = true;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;    // don't write
    desc.DepthFunc = D3D11_COMPARISON_LESS;               // only near z
    desc.StencilEnable = false;
    if (!createState(Render::eDepthState::TEST_BUT_NO_WRITE, desc, "ZCFG_TEST_BUT_NO_WRITE"))
        return false;

    // test for equal but don't write. Used to render on those pixels where
    // we have render previously like wireframes
    memset(&desc, 0x00, sizeof(desc));
    desc.DepthEnable = true;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;    // don't write
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    desc.StencilEnable = false;
    if (!createState(Render::eDepthState::TEST_EQUAL, desc, "ZCFG_TEST_EQUAL"))
        return false;

    // Render only if the ztest fails
    memset(&desc, 0x00, sizeof(desc));
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
    desc.StencilEnable = FALSE;
    if (!createState(Render::eDepthState::TEST_INVERTED, desc, "ZCFG_TEST_INVERTED"))
        return false;

    return true;
  }

  void destroyDepthStates() {
    for (auto c : z_cfgs)
      SAFE_RELEASE(c);
  }


  // -------------------------------------------------------------------
  bool createRasterizationStates() {
    PROFILE_SCOPED_NAMED("createRasterizationStates");

    rasterize_states[(int)Render::eRSConfig::DEFAULT] = nullptr;

    auto createState = [&](Render::eRSConfig cfg, const D3D11_RASTERIZER_DESC& desc, const char* title) {
        HRESULT hr = device->CreateRasterizerState(&desc, &rasterize_states[(int)cfg]);
        if (FAILED(hr))
            return false;
        setDbgName(rasterize_states[(int)cfg], title);
        return true;
    };

    // Depth bias options when rendering the shadows
    D3D11_RASTERIZER_DESC desc = {
      D3D11_FILL_SOLID, // D3D11_FILL_MODE FillMode;
      D3D11_CULL_BACK,  // D3D11_CULL_MODE CullMode;
      FALSE,            // BOOL FrontCounterClockwise;
      13,               // INT DepthBias;
      0.0f,             // FLOAT DepthBiasClamp;
      2.0f,             // FLOAT SlopeScaledDepthBias;
      TRUE,             // BOOL DepthClipEnable;
      FALSE,            // BOOL ScissorEnable;
      FALSE,            // BOOL MultisampleEnable;
      FALSE,            // BOOL AntialiasedLineEnable;
    };
    if (!createState(Render::eRSConfig::SHADOWS, desc, "RS_SHADOWS"))
        return false;

    // --------------------------------------------------
    // No culling at all
    desc = D3D11_RASTERIZER_DESC {
      D3D11_FILL_SOLID, // D3D11_FILL_MODE FillMode;
      D3D11_CULL_NONE,  // D3D11_CULL_MODE CullMode;
      FALSE,            // BOOL FrontCounterClockwise;
      0,                // INT DepthBias;
      0.0f,             // FLOAT DepthBiasClamp;
      0.0,              // FLOAT SlopeScaledDepthBias;
      TRUE,             // BOOL DepthClipEnable;
      FALSE,            // BOOL ScissorEnable;
      FALSE,            // BOOL MultisampleEnable;
      FALSE,            // BOOL AntialiasedLineEnable;
    };
    if (!createState(Render::eRSConfig::CULL_NONE, desc, "RS_CULL_NONE"))
        return false;

    // Culling is reversed. Used when rendering the light volumes
    desc.CullMode = D3D11_CULL_FRONT;
    if (!createState(Render::eRSConfig::REVERSE_CULLING, desc, "RS_REVERSE_CULLING"))
        return false;

    // Wireframe and default culling back
    desc.FillMode = D3D11_FILL_WIREFRAME;
    desc.CullMode = D3D11_CULL_NONE;
    if (!createState(Render::eRSConfig::WIREFRAME, desc, "RS_WIREFRAME"))
        return false;

    return true;
  }

  void destroyRasterizeStates() {
    for (auto c : rasterize_states)
      SAFE_RELEASE(c);
  }

  bool createBlendStates() {
    PROFILE_SCOPED_NAMED("createBlendStates");

    blend_states[(int)Render::eBlendConfig::DEFAULT] = nullptr;

    D3D11_BLEND_DESC desc;

    auto createState = [&](Render::eBlendConfig cfg, const D3D11_BLEND_DESC& desc, const char* title) {
        HRESULT hr = device->CreateBlendState(&desc, &blend_states[(int)cfg]);
        if (FAILED(hr))
            return false;
        setDbgName(blend_states[(int)cfg], title);
        return true;
    };

    // Combinative blending
    memset(&desc, 0x00, sizeof(desc));
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    if (!createState(Render::eBlendConfig::COMBINATIVE, desc, "BLEND_COMBINATIVE"))
        return false;

    // Additive blending
    memset(&desc, 0x00, sizeof(desc));
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;      // Color must come premultiplied
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    if (!createState(Render::eBlendConfig::ADDITIVE, desc, "BLEND_ADDITIVE"))
        return false;

    // Additive blending controlled by src alpha
    memset(&desc, 0x00, sizeof(desc));
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    if (!createState(Render::eBlendConfig::ADDITIVE_BY_SRC_ALPHA, desc, "BLEND_ADDITIVE_BY_SRC_ALPHA"))
        return false;
    return true;
  }

  void destroyBlendStates() {
    for (int i = 0; i < (int)Render::eBlendConfig::COUNT; ++i)
      SAFE_RELEASE(blend_states[i]);
  }

  const D3D11_SHADER_INPUT_BIND_DESC* TPipelineState::CShaderBase::CShaderReflectionInfo::findBindPoint(const char* name) const {
    uint32_t name_id = TStrID(name);
    for (auto& br : bound_resources) {
      if (br.BindCount == name_id)
        return &br;
    }
    return nullptr;
  }

  static void destroyBackBuffer() {
    rt_backbuffer->destroyRT();
  }

  // -------------------------------------------------------------------
  namespace Internal {

    std::vector< IDXGIAdapter1* > getAdapters() {
      std::vector< IDXGIAdapter1* > adapters;
      IDXGIFactory1* factory = nullptr;
      if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)))
        return adapters;

      IDXGIAdapter1* adapter = nullptr;
      for (UINT i = 0; !FAILED(factory->EnumAdapters1(i, &adapter)); ++i) {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);
        TStr256 name;
        wcstombs(name.data(), desc.Description, name.capacity());
        int nbits = 20;
        dbg("Device %d : %s VRAM:%dMB Sys:%dMB Shared:%dMB\n", i, name.c_str(), desc.DedicatedVideoMemory >> nbits, desc.DedicatedSystemMemory >> nbits, desc.SharedSystemMemory >> nbits);
        adapters.push_back(adapter);
      }

      factory->Release();
      return adapters;
    }

    bool createDevice(HWND hWnd) {
      PROFILE_SCOPED_NAMED("createDevice");

      auto adapters = getAdapters();

      // Get client size
      RECT rc;
      GetClientRect(hWnd, &rc);
      xres = rc.right - rc.left;
      yres = rc.bottom - rc.top;

      // Create device
      DXGI_SWAP_CHAIN_DESC sd;
      ZeroMemory(&sd, sizeof(sd));
      sd.BufferCount = 1u;
      sd.BufferDesc.Width = xres;
      sd.BufferDesc.Height = yres;
      sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      sd.BufferDesc.RefreshRate.Numerator = 60;
      sd.BufferDesc.RefreshRate.Denominator = 1;
      sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      sd.OutputWindow = hWnd;
      sd.SampleDesc.Count = 1;
      sd.SampleDesc.Quality = 0;
      sd.Windowed = TRUE; // app.isFullScreen() ? FALSE : TRUE;
      sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
      sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

      // https://www.gamedev.net/forums/topic/696249-dxgi_swap_effect_discard-to-dxgi_swap_effect_flip_discard/
      // Use the new FLIP_DISCARD
      sd.BufferCount = 2u;
      sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

      UINT createDeviceFlags = 0;
#if defined( DEBUG )
      createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
      D3D_FEATURE_LEVEL desiredFeatureLevel = D3D_FEATURE_LEVEL_11_0;
      D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
      D3D_DRIVER_TYPE driver_type = D3D_DRIVER_TYPE_HARDWARE;

      IDXGIAdapter* adapter = nullptr;
      if (adapters.size() > 2) {
        adapter = adapters[0];
        driver_type = D3D_DRIVER_TYPE_UNKNOWN;
      }

      //D3D_DRIVER_TYPE_UNKNOWN
      HRESULT hr = D3D11CreateDeviceAndSwapChain(adapter,
        driver_type, nullptr, createDeviceFlags, &desiredFeatureLevel, 1,
        D3D11_SDK_VERSION, &sd, &swap_chain, &device, &featureLevel, &ctx);

      if (FAILED(hr))
        return false;

      setDbgName(device, "Dev");
      setDbgName(ctx, "Ctx");

      return true;
    }

    // ------------------------------------
    bool createDepthBuffer(int width, int height, DXGI_FORMAT in_fmt,
      ID3D11DepthStencilView** depth_stencil_view_ptr,
      ID3D11ShaderResourceView** shader_resource_view_ptr
    ) {
      PROFILE_SCOPED_NAMED("createDepthBuffer");
      ID3D11Texture2D* depth_stencil = nullptr;
      assert(width > 0);
      assert(height > 0);

      DXGI_FORMAT res_fmt = in_fmt;
      DXGI_FORMAT dsv_fmt = in_fmt;
      DXGI_FORMAT srv_fmt = in_fmt;
      if (in_fmt == DXGI_FORMAT_R32_FLOAT) {
        srv_fmt = DXGI_FORMAT_R32_FLOAT;
        res_fmt = DXGI_FORMAT_R32_TYPELESS;
        dsv_fmt = DXGI_FORMAT_D32_FLOAT;
      }

      // Create depth stencil texture
      D3D11_TEXTURE2D_DESC descDepth;
      ZeroMemory(&descDepth, sizeof(descDepth));
      descDepth.Width = width;
      descDepth.Height = height;
      descDepth.MipLevels = 1;
      descDepth.ArraySize = 1;
      descDepth.Format = res_fmt;
      descDepth.SampleDesc.Count = 1;
      descDepth.SampleDesc.Quality = 0;
      descDepth.Usage = D3D11_USAGE_DEFAULT;
      descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
      descDepth.CPUAccessFlags = 0;
      descDepth.MiscFlags = 0;

      if (shader_resource_view_ptr)
        descDepth.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

      HRESULT hr = device->CreateTexture2D(&descDepth, NULL, &depth_stencil);
      if (FAILED(hr))
        return false;

      // Create the depth stencil view
      D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
      ZeroMemory(&descDSV, sizeof(descDSV));
      descDSV.Format = dsv_fmt;
      descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
      descDSV.Texture2D.MipSlice = 0;
      hr = device->CreateDepthStencilView(depth_stencil, &descDSV, depth_stencil_view_ptr);
      if (FAILED(hr))
        return false;

      // Setup the description of the shader resource view.
      if (shader_resource_view_ptr) {

        D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
        shaderResourceViewDesc.Format = srv_fmt;
        shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceViewDesc.Texture2D.MipLevels = descDepth.MipLevels;

        // Create the shader resource view.
        hr = device->CreateShaderResourceView(depth_stencil, &shaderResourceViewDesc, shader_resource_view_ptr);
        if (FAILED(hr))
          return false;
      }

      depth_stencil->Release();
      return true;
    }

  }

  //// --------------------------------------
  bool createBackBuffer() {
    PROFILE_SCOPED_NAMED("createRT");
    if (!rt_backbuffer)
      rt_backbuffer = new Render::TRenderToTexture;
    else
      rt_backbuffer->destroyRT();

    // createRenderTarget()
    assert(swap_chain);

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = NULL;
    HRESULT hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr))
      return false;
    hr = device->CreateRenderTargetView(pBackBuffer, NULL, &rt_backbuffer->render_target_view);
    pBackBuffer->Release();
    setDbgName(rt_backbuffer->render_target_view, "BackBuffer");

    // Create a depth buffer
    if (!Internal::createDepthBuffer(xres, yres, DXGI_FORMAT_D24_UNORM_S8_UINT, &rt_backbuffer->depth_stencil_view, nullptr))
      return false;
    //setDbgName(rt_backbuffer->depth_srv, "ZBackBufferSRV");
    setDbgName(rt_backbuffer->depth_stencil_view, "ZBackBuffer");

    D3D11_TEXTURE2D_DESC desc = {};
    pBackBuffer->GetDesc(&desc);
    rt_backbuffer->width = desc.Width;
    rt_backbuffer->height = desc.Height;

    RenderPlatform::xres = desc.Width;
    RenderPlatform::yres = desc.Height;

    return true;
  }

  bool resizeBackBuffer(int new_width, int new_height) {
    //width = new_width;
    //height = new_height;
    destroyBackBuffer();
    //dbg("Resizing backbuffer to %dx%d\n", width, height);
    HRESULT hr = swap_chain->ResizeBuffers(0, (UINT)new_width, (UINT)new_height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
      return false;
    if (!createBackBuffer())
      return false;

    // Update render targets using the backbuffer as dimension
    for (auto& rp : Render::render_passes) {
      if (!rp->usesBackBuffer())
        continue;
      rp->destroy();
      rp->create();
    }

    return true;
  }

  void TRenderToTexture::destroyRT() {
    SAFE_RELEASE(render_target_view);
    SAFE_RELEASE(depth_stencil_view);
    SAFE_RELEASE(depth_srv);
  }

  void TRenderToTexture::activateRT() {
    RenderPlatform::ctx->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);
  }

  void TRenderToTexture::clearRenderTarget(VEC4 color) {
    RenderPlatform::ctx->ClearRenderTargetView(render_target_view, &color.x);
  }

  void TRenderToTexture::clearDepth(float clearDepthValue) {
    RenderPlatform::ctx->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH, clearDepthValue, 0);
  }

  static DXGI_FORMAT getFormatByName(const std::string& name) {
    if (name == "float3") return DXGI_FORMAT_R32G32B32_FLOAT;
    else if (name == "float4") return DXGI_FORMAT_R32G32B32A32_FLOAT;
    else if (name == "float2") return DXGI_FORMAT_R32G32_FLOAT;
    fatal("Unknown format name '%s'\n", name.c_str());
    return DXGI_FORMAT_UNKNOWN;
  }

  uint32_t TVertexDecl::expectedSize() const {
    uint32_t sz = 0;
    D3D11_INPUT_ELEMENT_DESC* e = static_cast<D3D11_INPUT_ELEMENT_DESC*>(elems);
    for( size_t i=0; i<num_elems; ++i, ++e) {
      switch (e->Format) {
      case DXGI_FORMAT_R32G32_FLOAT: sz += 2 * sizeof(float); break;
      case DXGI_FORMAT_R32G32B32_FLOAT: sz += 3 * sizeof(float); break;
      case DXGI_FORMAT_R32G32B32A32_FLOAT: sz += 4 * sizeof(float); break;
      default:
        fatal("Don't know the size of format DXGI_FORMAT %08x. Semantic: %s\n", e->Format, e->SemanticName);
      }
    }
    return sz;
  }

  void TVertexDecl::destroy() {
    if (elems)
      delete[] elems;
    elems = nullptr;
    num_elems = 0;
  }

  bool TVertexDecl::create(const json& j) {
    assert(num_elems == 0);
    const json& jelems = j["elems"];
    std::vector< D3D11_INPUT_ELEMENT_DESC > velems;
    velems.reserve(jelems.size());
    eachArr(jelems, [this,&velems](const json& j, size_t idx) {

      D3D11_INPUT_ELEMENT_DESC s;
      memset(&s, 0x00, sizeof(s));
      std::string name = j.value("name", "");
      assert(!name.empty());
      s.SemanticName = _strdup(name.c_str());
      s.SemanticIndex = j.value("idx", 0);
      s.InputSlot = j.value("slot", 0);
      s.Format = getFormatByName(j.value("type", ""));
      s.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
      s.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
      s.InstanceDataStepRate = j.value("rate", 0);
      velems.push_back(s);
      });

    num_elems = velems.size();
    elems = new D3D11_INPUT_ELEMENT_DESC[velems.size()];
    memcpy(elems, velems.data(), num_elems * sizeof(D3D11_INPUT_ELEMENT_DESC));
    return true;
  }

  // -----------------------------------------------------
  void destroy() {
    destroySamplers();
    destroyRasterizeStates();
    destroyDepthStates();
    destroyBlendStates();
    destroyBackBuffer();
    SAFE_RELEASE(ctx);
    SAFE_RELEASE(swap_chain);
    SAFE_RELEASE(device);
  }

  bool create(HWND hWnd) {
    PROFILE_SCOPED_NAMED("renderCreate");

    using namespace Internal;

    if (!createDevice(hWnd))
      return false;

    if (!createBackBuffer())
      return false;

    if (!createDepthStates())
      return false;

    if (!createRasterizationStates())
      return false;

    if (!createBlendStates())
      return false;

    if (!createSamplers())
      return false;

    return true;
  }

  void swapFrames() {
    PROFILE_SCOPED_NAMED("swapFrames");
    assert(swap_chain);
    swap_chain->Present(0, 0);
  }

  void beginFrame() {
  }

  void imGuiShutdown() {
    ImGui_ImplWin32_Shutdown();
    ImGui_ImplDX11_Shutdown();
  }

  void imGuiInit() {
    auto& app = CApp::get();
    if (!ImGui_ImplWin32_Init(app.getHWnd()))
      return;
    ImGui_ImplDX11_Init(RenderPlatform::device, RenderPlatform::ctx);
    app.wndProcHandler = ImGui_ImplWin32_WndProcHandler;
  }

  void imGuiBegin() {
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
  }

  //--------------------------------------------------------------------------------------
  //--------------------------------------------------------------------------------------
  // Helper for compiling shaders with D3DX11
  //--------------------------------------------------------------------------------------
  //--------------------------------------------------------------------------------------
  static CCacheManager cached_shaders("Cache Shaders", "data/cache/");



  // https://github.com/TheRealMJP/BakingLab/blob/master/SampleFramework11/v1.02/Graphics/ShaderCompilation.cpp#L131
  class FrameworkInclude : public ID3DInclude {
  public:
    std::vector<const char*> paths;

    HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override {
      for( auto& path : paths ) {
        std::string filepath = std::string(path) + std::string(pFileName);
        TBuffer b(filepath.c_str());
        if (b.empty())
          continue;;
        *pBytes = UINT(b.size());
        void* data = std::malloc(*pBytes);
        if( data ) 
          memcpy(data, b.data(), b.size());
        *ppData = data;
        return S_OK;
      }
      return E_FAIL;
    }

    HRESULT Close(LPCVOID pData) override
    {
      std::free(const_cast<void*>(pData));
      return S_OK;
    }
  };

  bool compileShaderFromFile(
    const char* szFileName,
    const char* szEntryPoint,
    const char* szShaderModel,
    TBuffer& oBuffer,
    std::string* error_msg_returned = nullptr
  ) {
    std::string strName(szFileName);
    TFileContext fc(strName);
    PROFILE_SCOPED_NAMED(strName);

    // cacheName should include file,Fn,SM,defines,timestamp of dependend files
    std::string cacheName = strName + std::string(TTempStr("_Fn_%s_SM_%s", szEntryPoint, szShaderModel));
    if (cached_shaders.isCached(strName, cacheName, oBuffer))
      return true;

    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    dwShaderFlags |= D3DCOMPILE_DEBUG;
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
    //dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;

    // To avoid having to transpose the matrix in the ctes buffers when
    // uploading matrix to the gpu, and to keep multiplying vector * matrix
    // in the shaders instead of matrix * vector (as in gles20)
    dwShaderFlags |= D3D10_SHADER_PACK_MATRIX_ROW_MAJOR;

    wchar_t wFilename[MAX_PATH];
    mbstowcs(wFilename, szFileName, MAX_PATH);

    // Example of defines
    const D3D_SHADER_MACRO defines[] = {
      "PLATFORM_HLSL", "1",
      NULL, NULL
    };

    ID3DBlob* pBlobOut = nullptr;

    FrameworkInclude appIncludes;
    TParsedFilename pf(szFileName);
    appIncludes.paths.push_back(pf.path.c_str());
    if( pf.path != "data/shaders/" )
      appIncludes.paths.push_back("data/shaders/");
    appIncludes.paths.push_back("./");

    while (true) {
      ID3DBlob* pErrorBlob = nullptr;
      hr = D3DCompileFromFile(
        wFilename
        , defines
        , &appIncludes
        , szEntryPoint
        , szShaderModel
        , dwShaderFlags
        , 0
        , &pBlobOut
        , &pErrorBlob);

      if (pErrorBlob) {
        dbg("Compiling %s: %s", szFileName, pErrorBlob->GetBufferPointer());
      }

      if (FAILED(hr))
      {
        const char* msg = "Unknown error";

        if (pErrorBlob != NULL) {
          msg = (char*)pErrorBlob->GetBufferPointer();
        }
        else if (hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
          msg = "Shader path of input file not found";
        }
        else if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
          msg = "Shader input file not found";
        }

        bool must_retry = true;

        // If the just want the error msg, save it and return
        if (error_msg_returned) {
          *error_msg_returned = msg;
          must_retry = false;
        }
        else {
          fatal("Error compiling shader %s:\n%s\n", szFileName, msg);
        }

        if (pErrorBlob) pErrorBlob->Release();
        if (!must_retry)
          return false;

        continue;
      }
      if (pErrorBlob) pErrorBlob->Release();
      break;
    }

    oBuffer.resize(pBlobOut->GetBufferSize());
    memcpy(oBuffer.data(), pBlobOut->GetBufferPointer(), pBlobOut->GetBufferSize());

    cached_shaders.cache(strName, cacheName, oBuffer);

    return true;
  }

  FORCEINLINE HRESULT
    D3D11Reflect(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
      _In_ SIZE_T SrcDataSize,
      _Out_ ID3D11ShaderReflection** ppReflector) {
    return D3DReflect(pSrcData, SrcDataSize,
      IID_ID3D11ShaderReflection, (void**)ppReflector);
  }

  bool TPipelineState::CShaderBase::scanResourcesFrom(const TBuffer& blob) {
    HRESULT hr;
    ID3D11ShaderReflection* reflection = nullptr;
    {
      PROFILE_SCOPED_NAMED("Reflection");
      hr = D3D11Reflect(blob.data(), blob.size(), &reflection);
      if (FAILED(hr))
        return false;
    }

    PROFILE_SCOPED_NAMED("Parse");
    D3D11_SHADER_DESC desc;
    reflection->GetDesc(&desc);

    reflection_info = new CShaderReflectionInfo;
    reflection_info->bound_resources.clear();
    for (unsigned int i = 0; i < desc.BoundResources; ++i) {
      D3D11_SHADER_INPUT_BIND_DESC sib_desc;
      hr = reflection->GetResourceBindingDesc(i, &sib_desc);
      if (FAILED(hr))
        continue;
      sib_desc.BindCount = TStrID(sib_desc.Name);
      // Dupe the returned memory to keep a copy
      sib_desc.Name = _strdup(sib_desc.Name);
      reflection_info->bound_resources.push_back(sib_desc);
    }
    reflection->Release();
    return true;
  }

  // --------------------------------------------------------------------
  void TPipelineState::CVertexShader::destroy() {
    SAFE_RELEASE(vs);
    SAFE_RELEASE(vtx_layout);
  }

  bool TPipelineState::CVertexShader::compile(
    const char* szFileName
    , const char* szEntryPoint
    , const char* profile
    , const char* vertex_type_name
  ) {

    // If not given, use the existing one (if any)
    if (!vertex_type_name && !shader_vtx_type_name.empty())
      vertex_type_name = shader_vtx_type_name.c_str();

    // A blob is a representation of a buffer in dx, a void pointer + size
    TBuffer VSBlob;
    bool is_ok = compileShaderFromFile(szFileName, szEntryPoint
      , profile, VSBlob);
    if (!is_ok)
      return false;

    // Create the vertex shader
    HRESULT hr;
    hr = RenderPlatform::device->CreateVertexShader(
      VSBlob.data()
      , VSBlob.size()
      , nullptr
      , &vs);
    if (FAILED(hr))
      return false;

    setDbgName(vs, szEntryPoint);

    assert(vertex_type_name);
    auto decl = Render::TVertexDecl::getByName(vertex_type_name);
    assert(decl || fatal("Can't identify vertex decl named '%s'\n", vertex_type_name));

    // Create the input layout
    vtx_layout = nullptr;
    UINT nelems = (UINT)decl->num_elems;
    const D3D11_INPUT_ELEMENT_DESC* elems = static_cast<const D3D11_INPUT_ELEMENT_DESC*>(decl->elems);
    hr = RenderPlatform::device->CreateInputLayout(elems, nelems, VSBlob.data(),
      VSBlob.size(), &vtx_layout);
    if (FAILED(hr)) {
      fatal("Failed CreateInputLayout(%s,%d)\n", decl->getCName(), VSBlob.size());
      return false;
    }
    setDbgName(vtx_layout, decl->getCName());

    shader_src = szFileName;
    shader_fn = szEntryPoint;
    shader_profile = profile;
    shader_vtx_type_name = vertex_type_name;

    scanResourcesFrom( VSBlob );

    return true;
  }

  void TPipelineState::CVertexShader::activate() const {
    if (vtx_layout)
      ctx->IASetInputLayout(vtx_layout);
    if (vs)
      ctx->VSSetShader(vs, nullptr, 0);
  }

  // --------------------------------------------------------------------
  void TPipelineState::CPixelShader::destroy() {
    SAFE_RELEASE(ps);
  }

  bool TPipelineState::CPixelShader::compile(
    const char* szFileName
    , const char* szEntryPoint
    , const char* profile
  ) {

    TBuffer blob;
    bool is_ok = compileShaderFromFile(szFileName, szEntryPoint, profile, blob);
    if (!is_ok)
      return false;

    // Create the pixel shader
    HRESULT hr;
    hr = RenderPlatform::device->CreatePixelShader(
      blob.data()
      , blob.size()
      , NULL
      , &ps);
    if (FAILED(hr))
      return false;

    setDbgName(ps, szEntryPoint);

    shader_src = szFileName;
    shader_fn = szEntryPoint;
    shader_profile = profile;

    scanResourcesFrom(blob);

    return true;
  }

  void TPipelineState::CPixelShader::activate() const {
    ctx->PSSetShader(ps, nullptr, 0);
  }

  // --------------------------------------------------------------------
  void TPipelineState::CComputeShader::destroy() {
    SAFE_RELEASE(cs);
  }

  bool TPipelineState::CComputeShader::compile(
    const char* szFileName
    , const char* szEntryPoint
    , const char* profile
    , std::string* error_msg
  ) {

    TBuffer blob;
    bool is_ok = compileShaderFromFile(szFileName, szEntryPoint, profile, blob, error_msg);
    if (!is_ok)
      return false;

    HRESULT hr;

    // Create the compute shader
    {
      PROFILE_SCOPED_NAMED("CreateShader");
      hr = RenderPlatform::device->CreateComputeShader(
        blob.data()
        , blob.size()
        , nullptr
        , &cs);
      if (FAILED(hr))
        return false;
    }

    setDbgName(cs, szEntryPoint);

    scanResourcesFrom(blob);

    /*
        for (unsigned int i = 0; i < desc.InputParameters; ++i) {
          D3D11_SIGNATURE_PARAMETER_DESC desc;
          HRESULT hr = reflection->GetInputParameterDesc(i, &desc);
          if (!FAILED(hr)) {
            dbg("hi\n");
          }
        }

        for (unsigned int i = 0; i < desc.OutputParameters; ++i) {
          D3D11_SIGNATURE_PARAMETER_DESC desc;
          HRESULT hr = reflection->GetOutputParameterDesc(i, &desc);
          if (!FAILED(hr)) {
            dbg("hi\n");
          }
        }


        for (unsigned int i = 0; i < desc.ConstantBuffers; ++i) {
          unsigned int register_index = 0;
          ID3D11ShaderReflectionConstantBuffer* buffer = NULL;
          buffer = reflection->GetConstantBufferByIndex(i);
          D3D11_SHADER_BUFFER_DESC bdesc;
          buffer->GetDesc(&bdesc);

          for (unsigned int k = 0; k < desc.BoundResources; ++k)
          {
            D3D11_SHADER_INPUT_BIND_DESC ibdesc;
            reflection->GetResourceBindingDesc(k, &ibdesc);

            if (!strcmp(ibdesc.Name, bdesc.Name))
              register_index = ibdesc.BindPoint;
          }
          ////
          ////Add constant buffer
          ////
          //ConstantShaderBuffer* shaderbuffer = new ConstantShaderBuffer(register_index, Engine::String.ConvertToWideStr(bdesc.Name), buffer, &bdesc);
          //mShaderBuffers.push_back(shaderbuffer);
        }
        */

    shader_src = szFileName;
    shader_fn = szEntryPoint;
    shader_profile = profile;

    return true;
  }

  void TPipelineState::CComputeShader::activate() const {
    // Can't update a buffer if it's still bound as vb
    ID3D11ShaderResourceView* srv_nulls[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ctx->VSSetShaderResources(0, 6, srv_nulls);
    ctx->CSSetShader(cs, nullptr, 0);
    assert(reflection_info);

#ifdef _DEBUG
    for (auto& bd : reflection_info->bound_resources) {
      switch (bd.Type) {
      case D3D_SIT_CBUFFER: {
        ID3D11Buffer* buffer = nullptr;
        ctx->CSGetConstantBuffers(bd.BindPoint, 1, &buffer);
        assert(buffer != nullptr || fatal("Missing bind cs %s cte buffer in slot %d, name %s\n", shader_fn.c_str(), bd.BindPoint, bd.Name));
        break; }
      case D3D_SIT_UAV_APPEND_STRUCTURED:
      case D3D_SIT_UAV_CONSUME_STRUCTURED:
      case D3D_SIT_UAV_RWBYTEADDRESS:
      case D3D_SIT_UAV_RWSTRUCTURED:
      case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
      case D3D_SIT_UAV_RWTYPED: {
        ID3D11UnorderedAccessView* uav = nullptr;
        ctx->CSGetUnorderedAccessViews(bd.BindPoint, 1, &uav);
        if (!uav)
          dbg("Missing bind cs %s uav in slot %d, name %s\n", shader_fn.c_str(), bd.BindPoint, bd.Name);
        break; }
      case D3D_SIT_SAMPLER: {
        ID3D11SamplerState* sampler = nullptr;
        RenderPlatform::ctx->CSGetSamplers(bd.BindPoint, 1, &sampler);
        if (!sampler)
          dbg("Missing bind cs %s Sampler in slot %d, name %s\n", shader_fn.c_str(), bd.BindPoint, bd.Name);
        break; }
      case D3D_SIT_TEXTURE:
      case D3D_SIT_STRUCTURED:
      case D3D_SIT_BYTEADDRESS: {
        ID3D11ShaderResourceView* srv = nullptr;
        ctx->CSGetShaderResources(bd.BindPoint, 1, &srv);
        assert(srv != nullptr || fatal("Missing bind cs %s resource in slot %d, name %s\n", shader_fn.c_str(), bd.BindPoint, bd.Name));
        break; }
      default:
        fatal("Don't know how to check if resource %s (type:%d) of cs %s is bounded\n", bd.Name, bd.Type, shader_fn.c_str());
        break;
      }
    }
#endif
  }

  void TPipelineState::CComputeShader::deactivate() const {
    static const int max_uavs = 8;
    static const int max_slots = 16;
    ID3D11UnorderedAccessView* uavs_null[max_uavs] = {
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    };
    ID3D11ShaderResourceView* srvs_null[max_slots] = {
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    };
    // Null all cs params
    ctx->CSSetUnorderedAccessViews(0, max_uavs, uavs_null, nullptr);
    ctx->CSSetShaderResources(0, max_slots, srvs_null);
    ctx->CSSetShader(nullptr, nullptr, 0);
  }

  // -------------------------------------------------------------------------------------
  void TGPUBuffer::addRef() {
    assert(buffer);
    buffer->AddRef();
  }

  bool TGPUBuffer::copyGPUtoCPU(void* out_addr) const {

    // We need a staging buffer to read the contents of the gpu buffer.
    // Create a new buffer with the same format as the buffer of the gpu buffer, but with the staging & cpu read flags.
    D3D11_BUFFER_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    buffer->GetDesc(&desc);
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    ID3D11Buffer* cpu_buffer = nullptr;
    if (!FAILED(RenderPlatform::device->CreateBuffer(&desc, nullptr, &cpu_buffer))) {
      // Copy from the gpu buffer to the cpu buffer.
      RenderPlatform::ctx->CopyResource(cpu_buffer, buffer);

      assert(cpu_buffer);

      // Request access to read it.
      D3D11_MAPPED_SUBRESOURCE mr;
      if (RenderPlatform::ctx->Map(cpu_buffer, 0, D3D11_MAP_READ, 0, &mr) == S_OK) {

        if (out_addr) {
          memcpy(out_addr, mr.pData, desc.ByteWidth);
        }
        else {
          if (cpu_data.size() != desc.ByteWidth)
            ((TBuffer*)&cpu_data)->resize(desc.ByteWidth);
          memcpy((char*)cpu_data.data(), mr.pData, desc.ByteWidth);
        }
        RenderPlatform::ctx->Unmap(cpu_buffer, 0);
        cpu_buffer->Release();
        return true;
      }

      cpu_buffer->Release();
    }

    return false;
  }

  // --------------------------------------------------------------------
  void TShaderCte::destroy() {
    SAFE_RELEASE(cb);
  }

  bool TShaderCte::createPlatform(uint32_t nbytes) {
    // Create the constant buffer
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = nbytes;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    HRESULT hr = RenderPlatform::device->CreateBuffer(&bd, NULL, &cb);
    if (FAILED(hr))
      return false;
    return true;
  }

  bool TTexture::create2DPlatform(uint32_t width, uint32_t height, const void* raw_data, TTexture::eFormat fmt) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT(fmt);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.SampleDesc.Count = 1;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = raw_data;
    sd.SysMemPitch = desc.Width * 4;
    HRESULT hr = RenderPlatform::device->CreateTexture2D(&desc, &sd, &resource);
    if (FAILED(hr))
      return false;

    // Create a resource view so we can use it in the shaders as input
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = desc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = desc.MipLevels;
    hr = RenderPlatform::device->CreateShaderResourceView(resource, &SRVDesc, &resource_view);
    if (FAILED(hr))
      return false;

    return true;
  }

  bool TTexture::createPlatform(
    uint32_t new_xres, uint32_t new_yres,
    eFormat new_pixel_format,
    int new_nmipmaps,
    bool is_cubemap,
    bool is_render_target,
    bool generate_uav,
    bool is_dynamic
  ) {

    DXGI_FORMAT desc_fmt = DXGI_FORMAT(new_pixel_format);
    // Create a color surface
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = new_xres;
    desc.Height = new_yres;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = desc_fmt;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    if (generate_uav) {
      if (new_pixel_format == DXGI_FORMAT_B8G8R8A8_UNORM)
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    if (is_dynamic) {
      desc.Usage = D3D11_USAGE_DYNAMIC;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }

    if (is_render_target)
      desc.BindFlags |= D3D11_BIND_RENDER_TARGET;

    HRESULT hr = RenderPlatform::device->CreateTexture2D(&desc, NULL, &resource);
    if (FAILED(hr))
      return false;

    // Create a resource view so we can use it in the shaders as input
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    memset(&SRVDesc, 0, sizeof(SRVDesc));
    SRVDesc.Format = desc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = desc.MipLevels;
    hr = RenderPlatform::device->CreateShaderResourceView(resource, &SRVDesc, &resource_view);
    if (FAILED(hr))
      return false;

    if (generate_uav) {
      D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      ZeroMemory(&uav_desc, sizeof(uav_desc));
      uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
      uav_desc.Texture2D.MipSlice = 0;
      uav_desc.Format = DXGI_FORMAT_UNKNOWN;
      //uav_desc.Buffer.NumElements = num_elems;
      hr = RenderPlatform::device->CreateUnorderedAccessView(resource, &uav_desc, &uav);
      if (FAILED(hr))
        return false;
    }

    return true;
  }

  bool TTexture::create3DPlatform(
    uint32_t new_xres, uint32_t new_yres, uint32_t new_zres,
    eFormat new_pixel_format,
    int  new_nmipmaps,
    bool is_dynamic,
    bool is_render_target,
    bool generate_uav
  ) {

    DXGI_FORMAT format = DXGI_FORMAT(new_pixel_format);
    unsigned miscFlags = 0;

    D3D11_TEXTURE3D_DESC desc;
    desc.Width = static_cast<UINT>(new_xres);
    desc.Height = static_cast<UINT>(new_yres);
    desc.Depth = static_cast<UINT>(new_zres);
    desc.MipLevels = static_cast<UINT>(new_nmipmaps);
    desc.Format = format;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = miscFlags & ~D3D11_RESOURCE_MISC_TEXTURECUBE;

    if (generate_uav) {
      if (new_pixel_format == DXGI_FORMAT_B8G8R8A8_UNORM)
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    if (is_dynamic) {
      desc.Usage = D3D11_USAGE_DYNAMIC;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }
    if (is_render_target)
      desc.BindFlags |= D3D11_BIND_RENDER_TARGET;

    HRESULT hr = RenderPlatform::device->CreateTexture3D(&desc, nullptr, &resource3d);
    if (FAILED(hr))
      return false;

    // Create a resource view so we can use it in the shaders as input
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    memset(&SRVDesc, 0, sizeof(SRVDesc));
    SRVDesc.Format = format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    SRVDesc.Texture3D.MipLevels = (!new_nmipmaps) ? -1 : desc.MipLevels;
    hr = RenderPlatform::device->CreateShaderResourceView(resource3d, &SRVDesc, &resource_view);
    if (FAILED(hr))
      return false;

    if (generate_uav) {
      D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      ZeroMemory(&uav_desc, sizeof(uav_desc));
      uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
      uav_desc.Texture3D.MipSlice = 0;
      uav_desc.Texture3D.WSize = new_zres;
      uav_desc.Format = DXGI_FORMAT_UNKNOWN;
      //uav_desc.Buffer.NumElements = num_elems;
      hr = RenderPlatform::device->CreateUnorderedAccessView(resource3d, &uav_desc, &uav);
      if (FAILED(hr))
        return false;
    }

    return true;
  }

  void TTexture::destroyPlatform() {
    SAFE_RELEASE(uav);
    SAFE_RELEASE(resource_view);
    SAFE_RELEASE(resource);
    SAFE_RELEASE(resource3d);
  }

  void TMesh::addRef() {
    vb->AddRef();
    ib->AddRef();
  }

  bool TMesh::createIndexBuffer(uint32_t new_nindices, uint32_t new_bytes_per_index, const void* index_data, bool create_uav) {

    assert(new_nindices > 0);
    assert(new_bytes_per_index == 2 || new_bytes_per_index == 4);
    auto indices_bytes = new_bytes_per_index * new_nindices;
    if (index_data) {
      cpu_indices.resize(indices_bytes);
      memcpy(cpu_indices.data(), index_data, indices_bytes);
    }

    // Create the IB
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = indices_bytes;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;

    if (create_uav) {
      bd.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
      bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
      bd.StructureByteStride = new_bytes_per_index;
    }

    // The initial contents of the VB if something has been given
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    D3D11_SUBRESOURCE_DATA InitData;
    if (index_data) {
      ZeroMemory(&InitData, sizeof(InitData));
      InitData.pSysMem = index_data;
      pInitData = &InitData;
    }
    HRESULT hr = RenderPlatform::device->CreateBuffer(&bd, pInitData, &ib);
    if (FAILED(hr))
      return false;

    if (create_uav) {
      D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      ZeroMemory(&uav_desc, sizeof(uav_desc));
      uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
      uav_desc.Buffer.FirstElement = 0;
      uav_desc.Buffer.NumElements = bd.ByteWidth / 4;
      uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
      uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
      hr = RenderPlatform::device->CreateUnorderedAccessView(ib, &uav_desc, &uav_ib);
      if (FAILED(hr))
        return false;
    }

    return true;
  }

  static bool activateBufferInPipeline(const char* cte_name, const TGPUBuffer& buf, const TPipelineState* pipeline) {
    assert(pipeline);

    // Enable buffer in vs
    if (!pipeline->cs.isValid()) {
      assert(pipeline->vs.isValid());
      auto b = pipeline->vs.reflection_info->findBindPoint(cte_name);
      if (b)
        RenderPlatform::ctx->VSSetShaderResources(b->BindPoint, 1, &buf.srv);
      b = pipeline->ps.reflection_info->findBindPoint(cte_name);
      if (b)
        RenderPlatform::ctx->PSSetShaderResources(b->BindPoint, 1, &buf.srv);
      return true;
    }

    // cs
    auto b = pipeline->cs.reflection_info->findBindPoint(cte_name);
    if (!b)
      return false;
    assert(b);
    switch (b->Type) {
    case D3D_SIT_BYTEADDRESS:
    case D3D_SIT_STRUCTURED:
      assert(buf.srv);
      RenderPlatform::ctx->CSSetShaderResources(b->BindPoint, 1, &buf.srv);
      break;
    case D3D_SIT_UAV_RWBYTEADDRESS:
    case D3D_SIT_UAV_RWSTRUCTURED:
    case D3D_SIT_UAV_RWTYPED:
    case D3D_SIT_UAV_APPEND_STRUCTURED:
      assert(buf.uav);
      RenderPlatform::ctx->CSSetUnorderedAccessViews(b->BindPoint, 1, &buf.uav, nullptr);
      break;
    default:
      fatal("Invalid compute task parameter to assign as a generic buffer: %s\n", cte_name);
    }
    return true;
  }

}

namespace Render {

  // -------------------------------------------------------------------------------------
  void TGPUBuffer::setName(const std::string& new_name) {
    IResource::setName(new_name);
    if( buffer )
        setDbgName(buffer, new_name.c_str());
    if (srv)
        setDbgName(srv, (new_name + ".srv").c_str());
    if (uav)
        setDbgName(uav, (new_name + ".uav").c_str());
    if (cte.cb)
        setDbgName(cte.cb, (new_name + ".cte_cb").c_str());
  }

  // -------------------------------------------------------------------------------------
  void TMesh::setName(const std::string& new_name) {
    IResource::setName(new_name);
    setDbgName(vb, new_name.c_str());
    if (ib)
      setDbgName(ib, (new_name + ".ib").c_str());
    if (uav)
      setDbgName(uav, (new_name + "_vb.uav").c_str());
    if (uav_ib)
      setDbgName(uav_ib, (new_name + "_ib.uav").c_str());
  }

  void TMesh::destroy() {
    SAFE_RELEASE(vb);
    SAFE_RELEASE(ib);
    SAFE_RELEASE(uav);
    SAFE_RELEASE(uav_ib);
  }

  bool TMesh::isValid() const {
    return IResource::isValid() && (vb != nullptr);
  }

  bool TMesh::create(
    ePrimitiveType new_prim_type
    , const void* vertex_data
    , uint32_t new_nvertex
    , const TVertexDecl* new_vertex_decl
    , const void* index_data
    , uint32_t new_nindices
    , uint32_t new_bytes_per_index
    , uint32_t flags
  ) {
    assert(new_vertex_decl);
    assert(new_nvertex > 0);

    this->nvertices = new_nvertex;
    this->primitive_type = new_prim_type;
    this->vertex_decl = new_vertex_decl;
    uint32_t vertex_bytes = new_nvertex * vertex_decl->bytes_per_vertex;

    if (vertex_data) {
      cpu_vertices.resize(vertex_bytes);
      memcpy(cpu_vertices.data(), vertex_data, vertex_bytes);
    }

    bool create_uav = new_prim_type == ePrimitiveType::ePoint
      || (flags & FLAG_ACCESS_FROM_COMPUTE);

    bool create_srv = !create_uav;

    // Create the VB
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = vertex_decl->bytes_per_vertex * nvertices;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    if (create_uav) {
      bd.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
      bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
      bd.StructureByteStride = vertex_decl->bytes_per_vertex;
    }

    if (create_srv) {
      bd.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
      bd.StructureByteStride = vertex_decl->bytes_per_vertex;
      bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    }

    // The initial contents of the VB if something has been given
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = vertex_data;
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    if (vertex_data)
      pInitData = &InitData;
    HRESULT hr = RenderPlatform::device->CreateBuffer(&bd, pInitData, &vb);
    if (FAILED(hr))
      return false;

    if (index_data) {
      nindices = new_nindices;
      bytes_per_index = new_bytes_per_index;
      if (!createIndexBuffer(nindices, bytes_per_index, index_data, create_uav))
        return false;
    }

    if (create_uav) {
      D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
      ZeroMemory(&uav_desc, sizeof(uav_desc));
      uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
      uav_desc.Buffer.FirstElement = 0;
      uav_desc.Buffer.NumElements = bd.ByteWidth / 4;
      uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
      uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
      hr = RenderPlatform::device->CreateUnorderedAccessView(vb, &uav_desc, &uav);
      if (FAILED(hr))
        return false;
    }

    if (create_srv) {
      D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
      srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
      srv_desc.BufferEx.FirstElement = 0;
      srv_desc.Format = DXGI_FORMAT_UNKNOWN;
      srv_desc.Buffer.ElementWidth = vertex_decl->bytes_per_vertex;
      srv_desc.BufferEx.NumElements = nvertices;
      hr = RenderPlatform::device->CreateShaderResourceView(vb, &srv_desc, &srv);
      if (FAILED(hr)) 
        return false;
    }


    TRawMesh::TGroup g;
    g.count = ib ? nindices : nvertices;
    g.first = 0;
    groups.clear();
    groups.push_back(g);

    setResourceClass(getResourceClassOfObj<TMesh>());

    return true;
  }

  // -------------------------------------------------------------------------------------
  bool TShaderCteBase::createBase(uint32_t new_slot, uint32_t nbytes, const char* new_label) {
    assert(cb == nullptr);
    slot = new_slot;

    if (!createPlatform(nbytes))
      return false;

    cpu_vertices.resize(nbytes);
    is_dirty = false;

    setDbgName(cb, new_label);
    return true;
  }

  bool TShaderCteBase::createBaseArray(uint32_t new_slot, uint32_t nbytes_per_instance, uint32_t ninstances, const char* new_label) {
    assert(cb == nullptr);
    slot = new_slot;
    if (!createPlatform(nbytes_per_instance))
      return false;
    cpu_vertices.resize(nbytes_per_instance * ninstances);
    is_dirty = false;
    setDbgName(cb, new_label);
    return true;
  }


  // In DX11, we are not really uploading to GPU, just copying to our cpu shadow copy
  void TShaderCteBase::uploadToGPUBase(const void* data, uint32_t data_bytes, uint32_t dst_offset) {
    assert(cb);
    assert(data);
    assert(dst_offset + data_bytes <= cpu_vertices.size());
    memcpy(cpu_vertices.data() + dst_offset, data, data_bytes);
    is_dirty = true;
  }

  // -------------------------------------------------------------------------------------
  void TTexture::destroy() {
    TTexture::destroyPlatform();
  }

  void TTexture::setName(const std::string& new_name) {
    IResource::setName(new_name);
    if (resource)
      setDbgName(resource, new_name.c_str());
    if (resource_view)
      setDbgName(resource_view, new_name.c_str());
  }

  bool TTexture::updateGPUFromCPU(const void* src, size_t nbytes) {
    D3D11_MAPPED_SUBRESOURCE ms;
    auto ctx = RenderPlatform::getContext();
    ID3D11Resource* res = resource;
    if (resource3d)
      res = resource3d;

    assert(res);
    HRESULT hr = ctx->Map(res, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    if (FAILED(hr))
      return false;

    if (resource3d) {
      // Can't assume the layout in memory is pure linear, so we must honor
      // the info inside the ms struct (Under RenderDoc, the memory appears 
      // to be fully linear)
      const uint8_t* s = (const uint8_t*)src;
      
      D3D11_TEXTURE3D_DESC desc3d;
      resource3d->GetDesc(&desc3d);

      size_t s_row_bytes = desc3d.Width;
      size_t s_depth_bytes = desc3d.Width * desc3d.Height;
      if ( desc3d.Format == DXGI_FORMAT_B8G8R8A8_UNORM 
        || desc3d.Format == DXGI_FORMAT_R8G8B8A8_UNORM 
        || desc3d.Format == DXGI_FORMAT_R32_FLOAT
        ) {
        s_depth_bytes *= 4;
        s_row_bytes *= 4;
      } else if( desc3d.Format == DXGI_FORMAT_R16_FLOAT ) {
        s_depth_bytes *= 2;
        s_row_bytes *= 2;
      } else if (desc3d.Format == DXGI_FORMAT_R8_UNORM) {
        // ok
      }
      else {
        fatal("Unsupported format in TTexture3d::updateGPUFromCPU");
      }

      uint8_t* d = (uint8_t*)ms.pData;
      for (UINT z = 0; z < desc3d.Depth; ++z) {
        const uint8_t* sr = s;
        uint8_t* dr = d;
        for (UINT y = 0; y < desc3d.Height; ++y) {
          memcpy(dr, sr, s_row_bytes);
          dr += ms.RowPitch;
          sr += s_row_bytes;
        }
        s += s_depth_bytes;
        d += ms.DepthPitch;
      }
    }
    else {
      memcpy(ms.pData, src, nbytes);
    }

    ctx->Unmap(res, 0);
    return true;
  }

  void TTexture::updateSlice(const void* data, int new_width, int new_height, int total_bytes, int bytes_per_row, int mipmap_level, int slice, int bytes_per_pitch, const void* box) {
    ID3D11Resource* res = resource;
    if (!res)
      res = resource3d;
    assert(res);
    RenderPlatform::ctx->UpdateSubresource(
      res,
      0,
      (const D3D11_BOX*)box,
      data,
      bytes_per_row,
      bytes_per_pitch
    );
  }

  bool TTexture::create(const TBuffer& buffer) {

    assert(resource == nullptr);
    assert(resource_view == nullptr);

    if (buffer.empty() || buffer.size() < 4)
      return false;

    setResourceClass(getResourceClassOfObj<TTexture>());

    const uint32_t DDS_MAGIC = 0x20534444; // "DDS "
    if (*(int*)buffer.data() == DDS_MAGIC) {
      HRESULT hr = DirectX::CreateDDSTextureFromMemory(
        RenderPlatform::device,
        buffer.data(),
        buffer.size(),
        (ID3D11Resource**)&resource,
        &resource_view
      );
      if (FAILED(hr))
        return false;
    }
    else {

      // Try png
      std::vector<uint8_t> decoded_img;
      unsigned png_error = lodepng::decode(decoded_img, width, height, buffer );
      if( !png_error ) {
        if( !create2DPlatform( width, height, decoded_img.data(), RenderPlatform::TTexture::FMT_RGBA_8UNORM))
          return false;
      } 
      else {
        // Try TGA
        TTGALoader tga;
        if (tga.parse(buffer.getNewMemoryDataProvider())) {
          if( !create2DPlatform( tga.width(), tga.height(), tga.texels(), RenderPlatform::TTexture::FMT_RGBA_8UNORM))
            return false;
        } else
          return false;
      }
    }

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    resource->GetDesc(&desc);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    ZeroMemory(&srv_desc, sizeof(srv_desc));
    resource_view->GetDesc(&srv_desc);

    width = desc.Width;
    height = desc.Height;
    pixel_format = eFormat(desc.Format);
    nmipmaps = srv_desc.Texture2D.MipLevels;

    return true;
  }

  // ---------------------------------------------------------------
  bool TRenderToTexture::createRT(
    const char* rt_name,
    int new_xres, int new_yres,
    TTexture::eFormat color_fmt, TTexture::eFormat depth_fmt
  ) {

    if (color_fmt != TTexture::eFormat::INVALID) {
      if (!TTexture::create(new_xres, new_yres, color_fmt, 1, (int)eOptions::RENDER_TARGET))
        return false;
      setName("RT_" + std::string(rt_name));

      // Create the render target view
      HRESULT hr = RenderPlatform::device->CreateRenderTargetView(resource, NULL, &render_target_view);
      if (FAILED(hr))
        return false;
      setDbgName(render_target_view, getCName());

      width = new_xres;
      height = new_yres;
    }

    if (depth_fmt != eFormat::INVALID) {
      if (!RenderPlatform::Internal::createDepthBuffer(new_xres, new_yres, DXGI_FORMAT(depth_fmt), &depth_stencil_view, &depth_srv))
        return false;
      setDbgName(depth_stencil_view, (std::string("Z_") + std::string(rt_name)).c_str());
      setDbgName(depth_srv, (std::string("Zsrv_") + std::string(rt_name)).c_str());

      // Expose depth_srv as a independent texture
      if (!depth_texture)
        depth_texture = new Render::TTexture;
      else
        depth_texture->destroy();
      depth_texture->resource_view = depth_srv;
      depth_texture->resource_view->AddRef();
      depth_texture->width = new_xres;
      depth_texture->height = new_yres;
      std::string z_new_name = std::string(rt_name) + "_z.texture";
      if (depth_texture->getName() != z_new_name) {
        depth_texture->setResourceClass(getResourceClassOfObj< Render::TTexture >());
        Resources.registerNew(depth_texture, z_new_name);
      }
    }

    // We register ourselves so we can be destroyed by the resources manager
    std::string new_name = std::string(rt_name) + ".texture";
    if (name != new_name) {
      setResourceClass(getResourceClassOfObj< Render::TTexture >());
      Resources.registerNew((TTexture*)this, new_name);
    }

    return true;
  }

  void TRenderToTexture::destroy() {
    TRenderToTexture::destroyRT();
    TTexture::destroy();
  }

  bool TRenderToTexture::isValid() const {
    return this->render_target_view != nullptr;
  }

  // -----------------------------------------------------------------
  bool TRenderPass::create() {

    int imposed_xres = cfg_width;
    int imposed_yres = cfg_height;

    // Each color...
    for (int i = 0; i < max_color_attachments; ++i) {
      TColorAttach& ca = colors[i];
      if (ca.enabled) {

        if (ca.flags & RP_USE_BACKBUFFER_COLOR) {
          assert(cfg_width == 0 && cfg_height == 0);
          if (!ca.rt) {
            ca.rt = RenderPlatform::rt_backbuffer;
            ca.rt->setResourceClass(getResourceClassOfObj< Render::TTexture >());
            Resources.registerNew(ca.rt, TStr64("%s_%d.texture", name, i).c_str());
          }
          imposed_xres = RenderPlatform::xres;
          imposed_yres = RenderPlatform::yres;
        }
        else {
          if (!ca.rt)
            ca.rt = new TRenderToTexture;
          else
            ca.rt->destroyRT();
          bool is_ok = ca.rt->createRT(TStr64("%s_%d", name, i), cfg_width, cfg_height, ca.format, TTexture::eFormat::INVALID);
          assert(is_ok);
        }
      }
    }

    // The depth
    TDepthAttach& da = depth;
    if (da.enabled) {
      if (!da.rt)
        da.rt = new TRenderToTexture;
      else
        da.rt->destroyRT();
      bool is_ok = da.rt->createRT(TStr64("%s_depth", name), imposed_xres, imposed_yres, TTexture::eFormat::INVALID, da.format);
      assert(is_ok);
    }

    width = imposed_xres;
    height = imposed_yres;

    return true;
  }

  // -----------------------------------------------------------------
  // Activates the render targets and clears the background
  void TRenderPass::beginPass() {
    Render::TCmdBuffer cmd;
    cmd.pushGroup(name);

    if (sets_render_targets) {
      ID3D11RenderTargetView* rtv[4] = { nullptr, nullptr, nullptr, nullptr };
      ID3D11DepthStencilView* depth_dsv = nullptr;
      if (depth.rt)
        depth_dsv = depth.rt->depth_stencil_view;
      for (int i = 0; i < 4; ++i) {
        if (colors[i].enabled)
          rtv[i] = colors[i].rt->render_target_view;
      }
      RenderPlatform::ctx->OMSetRenderTargets(4, rtv, depth_dsv);

      for (int i = 0; i < 4; ++i) {
        if (colors[i].enabled)
          RenderPlatform::ctx->ClearRenderTargetView(colors[i].rt->render_target_view, &colors[i].clear_color.x);
      }

      if (depth.enabled)
        RenderPlatform::ctx->ClearDepthStencilView(depth.rt->depth_stencil_view, D3D11_CLEAR_DEPTH, depth.clear_value, 0);
    }
  }

  // -----------------------------------------------------------------
  void TRenderPass::endPass() {
    Render::TCmdBuffer cmd;
    cmd.popGroup();
  }

  // -------------------------------------------------------------------------------------
  void TPipelineState::destroy() {
    vs.destroy();
    ps.destroy();
    cs.destroy();
  }

  void TPipelineState::onFileChanged(const std::string& filename) {
    bool force_reload = (filename == "data/shaders/common.fx");
    if (!cs.shader_src.empty()) {
      if (cs.shader_src == filename || force_reload) {
        json j = json::object();
        j["fx_src"] = filename;
        j["cs"] = cs.shader_fn;
        createForCompute(j);
      }
    }
    else {
      if (ps.shader_src == filename || force_reload)
        ps.compile(ps.shader_src.c_str(), ps.shader_fn.c_str(), ps.shader_profile.c_str());
      if (vs.shader_src == filename || force_reload) {
        CVertexShader nvs = vs;
        if (nvs.compile(vs.shader_src.c_str(), vs.shader_fn.c_str(), vs.shader_profile.c_str(), nullptr))
          vs = nvs;
      }
    }
  }

  void TPipelineState::eachUnassignedResource(std::function<void(const char*, uint32_t idx)> resolver) const {
    for (auto& bd : cs.reflection_info->bound_resources) {
      switch (bd.Type) {
      case D3D_SIT_CBUFFER: {
        ID3D11Buffer* buffer = nullptr;
        RenderPlatform::ctx->CSGetConstantBuffers(bd.BindPoint, 1, &buffer);
        if (buffer == nullptr)
          resolver(bd.Name, bd.BindPoint);
        break; }
      case D3D_SIT_UAV_APPEND_STRUCTURED:
      case D3D_SIT_UAV_CONSUME_STRUCTURED:
      case D3D_SIT_UAV_RWBYTEADDRESS:
      case D3D_SIT_UAV_RWSTRUCTURED:
      case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
      case D3D_SIT_UAV_RWTYPED: {
        ID3D11UnorderedAccessView* uav = nullptr;
        RenderPlatform::ctx->CSGetUnorderedAccessViews(bd.BindPoint, 1, &uav);
        if (uav == nullptr)
          resolver(bd.Name, bd.BindPoint);
        break; }
      case D3D_SIT_SAMPLER: {
        ID3D11SamplerState* sampler = nullptr;
        RenderPlatform::ctx->CSGetSamplers(bd.BindPoint, 1, &sampler);
        if (sampler == nullptr)
          resolver(bd.Name, bd.BindPoint);
        break; }
      case D3D_SIT_TEXTURE:
      case D3D_SIT_STRUCTURED:
      case D3D_SIT_BYTEADDRESS: {
        ID3D11ShaderResourceView* srv = nullptr;
        RenderPlatform::ctx->CSGetShaderResources(bd.BindPoint, 1, &srv);
        if (srv == nullptr)
          resolver(bd.Name, bd.BindPoint);
        break; }
      default:
        fatal("Don't know how to check if resource %s of cs is bounded. Type:%d\n", bd.Name, bd.Type);
        break;
      }
    }
  }


  // -------------------------------------------------------------------------------------
  bool TPipelineState::createForCompute(const json& j) {
    std::string cs_profile = j.value("profile", "cs_5_0");
    std::string cs_entry = j.value("cs", "");
    std::string fx_src = j.value("fx_src", "data/shaders/compute.fx");
    assert(!cs_entry.empty());
    assert(!fx_src.empty());
    setResourceClass(getResourceClassOfObj<TPipelineState>());
    return cs.compile(fx_src.c_str(), cs_entry.c_str(), cs_profile.c_str());
  }


  // -------------------------------------------------------------------------------------
  void TComputeTask::beginTasks() {
    GPU_SCOPED("Clear GPU Buffers");
    // Like the counter of number of corners detected
    for (auto& b : auto_cleared_buffers)
      b->clear();
  }

  void TGPUBuffer::copyFromGPU(const TGPUBuffer& other) {
    RenderPlatform::ctx->CopyResource(buffer, other.buffer);
  }

  bool TComputeTask::activateCte(const char* cte_name, const TShaderCteBase& cte) const {
    assert(pipeline);
    if (!pipeline->cs.reflection_info)
      return false;
    auto b = pipeline->cs.reflection_info->findBindPoint(cte_name);
    if (!b)
      return false;
    switch (b->Type) {
    case D3D_SIT_CBUFFER:
      assert(cte.cb != nullptr);

      if (cte.is_dirty) {
        RenderPlatform::ctx->UpdateSubresource(cte.cb, 0, nullptr, cte.cpu_vertices.data(), 0, 0);
        auto non_cte = const_cast<const TShaderCteBase&>(cte);;
        non_cte.is_dirty = false;
      }

      RenderPlatform::ctx->CSSetConstantBuffers(b->BindPoint, 1, &cte.cb);
      break;
    default:
      fatal("Invalid compute task parameter to assign as a cte buffer: %s\n", name);
    }
    return true;
  }

  bool TComputeTask::activateBuffer(const char* cte_name, const TGPUBuffer& buf) const {
    if( &buf == nullptr )
      return false;
    return RenderPlatform::activateBufferInPipeline(cte_name, buf, pipeline);
  }

  bool TComputeTask::activateMesh(const char* cte_name, const TMesh& mesh) const {
    assert(pipeline);
    auto b = pipeline->cs.reflection_info->findBindPoint(cte_name);
    if (!b)
      return false;
    assert(b);
    switch (b->Type) {
    case D3D_SIT_UAV_RWBYTEADDRESS:
    case D3D_SIT_UAV_RWSTRUCTURED:
    assert(mesh.uav);
    RenderPlatform::ctx->CSSetUnorderedAccessViews(b->BindPoint, 1, &mesh.uav, nullptr);
    break;
    default:
    fatal("Invalid compute task parameter to assign a mesh: %s\n", cte_name);
    }
    return true;
  }

  bool TComputeTask::activateTexture(const char* cte_name, const TTexture& texture) const {
    assert(pipeline);
    auto b = pipeline->cs.reflection_info->findBindPoint(cte_name);
    if (!b)
      return false;
    assert(b);
    switch (b->Type) {
    case D3D_SIT_TEXTURE:
      assert(texture.resource_view);
      RenderPlatform::ctx->CSSetShaderResources(b->BindPoint, 1, &texture.resource_view);
    break;
    default:
    fatal("Invalid compute task parameter to assign a texture: %s\n", cte_name);
    }
    return true;
  }

  bool TComputeTask::activateMeshIndex(const char* cte_name, const TMesh& mesh) const {
    assert(pipeline);
    auto b = pipeline->cs.reflection_info->findBindPoint(cte_name);
    if (!b)
      return false;
    assert(b);
    switch (b->Type) {
    case D3D_SIT_UAV_RWBYTEADDRESS:
    case D3D_SIT_UAV_RWSTRUCTURED:
      assert(mesh.uav_ib);
      RenderPlatform::ctx->CSSetUnorderedAccessViews(b->BindPoint, 1, &mesh.uav_ib, nullptr);
      break;
    default:
      fatal("Invalid compute task parameter to assign a mesh index: %s\n", cte_name);
    }
    return true;
  }

  // -------------------------------------------------------------------------------------
  void TComputeTask::runTaskIndirect(const TGPUBuffer& buffer_with_args, uint32_t offset) const {
    assert(pipeline);
    // Wrap it inside a debug group
    GPU_SCOPED(name.c_str());
    pipeline->cs.activate();
    RenderPlatform::ctx->DispatchIndirect(buffer_with_args.buffer, offset);
    pipeline->cs.deactivate();
  }

  // -------------------------------------------------------------------------------------
  void TComputeTask::runTaskWithGroups(int thread_groupsx, int thread_groupsy, int thread_groupsz) const {
    assert(pipeline);
    assert(thread_groupsx > 0);
    assert(thread_groupsy > 0);
    assert(thread_groupsz > 0);
    // Wrap it inside a debug group
    GPU_SCOPED(name.c_str());
    pipeline->cs.activate();
    static bool dump = false;
    if (dump) dbg("Binding of %d resources for cs %s\n", pipeline->cs.reflection_info->bound_resources.size(), pipeline->getCName());
    if (thread_groupsx < 65536 && thread_groupsy < 65536 && thread_groupsz < 65536)
      RenderPlatform::ctx->Dispatch(thread_groupsx, thread_groupsy, thread_groupsz);
    else
      dbg("Compute task: Too many thread groups! %d %d %d\n", thread_groupsx, thread_groupsy, thread_groupsz);
    pipeline->cs.deactivate();
  }

  // -------------------------------------------------------------------------------------
  // Executes it with the current group settings
  void TComputeTask::runTask() const {

    /*
    UINT zero = 0;

    // Associate the cte buffers
    for (auto b : this->buffers) {
      if (b->cte.cb) {
        const auto& cte = b->cte;
        RenderPlatform::ctx->CSSetConstantBuffers(cte.slot, 1, &cte.cb);
      }
    }

    int next_texture = 0;
    int next_buffer = 0;
    int next_mesh = 0;
    int idx = 0;
    for (auto& br : pipeline->cs.bound_resources) {
      // Textures as ro
      if (br.Type == D3D_SIT_TEXTURE) {
        assert(next_texture < textures.size());
        assert(br.BindCount == 1);
        if (dump) dbg("  Using ro texture %s for slot %s at point %d\n", textures[next_texture]->getCName(), br.Name, br.BindPoint);
        RenderPlatform::ctx->CSSetShaderResources(br.BindPoint, 1, &textures[next_texture]->resource_view);
        ++next_texture;
      }
      // Textures as rw
      else if (br.Type == D3D_SIT_UAV_RWTYPED) {
        if (dump) dbg("  Using rw texture %s for slot %s at point %d\n", textures[next_texture]->getCName(), br.Name, br.BindPoint);
        assert(br.BindCount == 1);
        if (next_texture < textures.size()) {
          assert(textures[next_texture]->uav || fatal("[%d/%d] For bounded resource at slot %d and name %s, can't bind the texture %s as requires the 'compute_output' flag\n", idx, pipeline->cs.bound_resources.size(), br.BindPoint, br.Name, textures[next_texture]->getCName()));
          RenderPlatform::ctx->CSSetUnorderedAccessViews(br.BindPoint, 1, &textures[next_texture]->uav, nullptr);
          ++next_texture;
        }
        else if (next_buffer < buffers.size()) {
          assert(buffers[next_buffer]->uav);
          UINT zero = 0;
          RenderPlatform::ctx->CSSetUnorderedAccessViews(br.BindPoint, 1, &buffers[next_buffer]->uav, &zero);
          ++next_buffer;
        }
      }
      // Buffers as ro
      else if (br.Type == D3D_SIT_STRUCTURED) {
        if (dump) dbg("  Using ro buffer %s for slot %s at point %d\n", buffers[next_buffer]->getCName(), br.Name, br.BindPoint);
        assert(br.BindCount == 1);
        assert(next_buffer < buffers.size());
        assert(buffers[next_buffer]->uav);
        RenderPlatform::ctx->CSSetShaderResources(br.BindPoint, 1, &buffers[next_buffer]->srv);
        ++next_buffer;
      }
      // Buffers as rw
      else if (br.Type == D3D_SIT_UAV_RWSTRUCTURED) {
        if (dump) dbg("  Using rw buffer %s for slot %s at point %d\n", buffers[next_buffer]->getCName(), br.Name, br.BindPoint);
        assert(br.BindCount == 1);
        if (next_buffer < buffers.size()) {
          assert(buffers[next_buffer]->uav);
          UINT zero = 0;
          RenderPlatform::ctx->CSSetUnorderedAccessViews(br.BindPoint, 1, &buffers[next_buffer]->uav, &zero);
          ++next_buffer;
        }
        else {
          assert(meshes[next_mesh]->uav);
          RenderPlatform::ctx->CSSetUnorderedAccessViews(br.BindPoint, 1, &meshes[next_mesh]->uav, nullptr);
          ++next_mesh;
        }
      }
      // Buffers as cte buffer
      else if (br.Type == D3D_SIT_CBUFFER) {
        if (dump) dbg("  Using cbuffer %s for slot %s at point %d\n", buffers[next_buffer]->getCName(), br.Name, br.BindPoint);
        // Done using the slot of each cte buffer
        //assert(br.BindCount == 1);
        //assert(next_buffer < buffers.size());
        //auto b = buffers[next_buffer];
        //if(b->buffer)
        //  RenderPlatform::ctx->CSSetConstantBuffers(br.BindPoint, 1, &b->buffer );
        ////else if( b->cte.cb )
        ////  RenderPlatform::ctx->CSSetConstantBuffers(br.BindPoint, 1, &b->cte.cb);
        //else {
        //  fatal("Invalid buffer to bind!\n");
        //}
        //++next_buffer;
      }
      // Buffers as rw
      else if (br.Type == D3D_SIT_UAV_RWBYTEADDRESS) {
        assert(br.BindCount == 1);
        if (next_mesh < meshes.size()) {
          auto m = meshes[next_mesh];
          assert(m->uav);
          RenderPlatform::ctx->CSSetUnorderedAccessViews(br.BindPoint, 1, &m->uav, nullptr);
          ++next_mesh;
        }
        else if (next_buffer < buffers.size()) {
          auto m = buffers[next_buffer];
          assert(m->uav);
          RenderPlatform::ctx->CSSetUnorderedAccessViews(br.BindPoint, 1, &m->uav, &zero);
          ++next_buffer;
        }
      }
      // Buffers as rw
      else if (br.Type == D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER) {
        if (dump) dbg("  Using rw structured buffer with counter %s for slot %s at point %d\n", buffers[next_buffer]->getCName(), br.Name, br.BindPoint);
        assert(br.BindCount == 1);
        assert(next_buffer < buffers.size());
        auto b = buffers[next_buffer];
        assert(b->uav);
        RenderPlatform::ctx->CSSetUnorderedAccessViews(br.BindPoint, 1, &b->uav, &zero);
        ++next_buffer;
      }
      // Buffers as rw
      else if (br.Type == D3D_SIT_UAV_APPEND_STRUCTURED) {
        if (dump) dbg("  Using rw append structured buffer %s for slot %s at point %d\n", buffers[next_buffer]->getCName(), br.Name, br.BindPoint);
        assert(br.BindCount == 1);
        assert(next_buffer < buffers.size());
        auto b = buffers[next_buffer];
        assert(b->uav);
        RenderPlatform::ctx->CSSetUnorderedAccessViews(br.BindPoint, 1, &b->uav, &zero);
        ++next_buffer;
      }
      else {
        fatal("Unsupported bounded buffer. %d\n", br.Type);
      }
      ++idx;
    }
    */

    int w = sizes[0];
    int h = sizes[1];
    int thread_groups[3] = { (w + groups[0] - 1) / groups[0], (h + groups[1] - 1) / groups[1], 1 };
    runTaskWithGroups(thread_groups[0], thread_groups[1], thread_groups[2]);
  }

  void TComputeTask::endTasks() {
    // Each cs has already been disabled
  }

  // -------------------------------------------------------------------------------------
  bool TGPUBuffer::createCte(int slot, const TBuffer& initial_data) {
    if (!cte.createBase(slot, total_bytes, getCName()))
      return false;
    buffer = cte.cb;
    assert(initial_data.size() == cte.cpu_vertices.size());
    memcpy(cte.cpu_vertices.data(), initial_data.data(), initial_data.size());
    buffer->AddRef();
    RenderPlatform::ctx->UpdateSubresource(cte.cb, 0, nullptr, cte.cpu_vertices.data(), 0, 0);
    return true;
  }

  // -------------------------------------------------------------------------------------
  void TGPUBuffer::destroy() {
    SAFE_RELEASE(cte.cb);
    members.clear();
    SAFE_RELEASE(buffer);
    SAFE_RELEASE(buffer_clean);
    SAFE_RELEASE(srv);
    SAFE_RELEASE(uav);
  }

  // -------------------------------------------------------------------------------------
  void TGPUBuffer::copyCPUtoGPUFrom(const void* new_data, size_t num_bytes) const {
    assert(num_bytes <= bytes_per_elem * num_elems);
    assert(buffer);
    assert(new_data);
    if (num_bytes < total_bytes)
    {
      TBuffer b;
      b.resize(total_bytes);
      memcpy(b.data(), new_data, num_bytes);
      RenderPlatform::ctx->UpdateSubresource(buffer, 0, nullptr, b.data(), 0, 0);
    }
    else
    {
      RenderPlatform::ctx->UpdateSubresource(buffer, 0, nullptr, new_data, 0, 0);
    }
  }

  bool TGPUBuffer::configure(uint32_t new_num_elems, Render::TGPUBuffer::eDataType new_data_type, const char* new_name, bool new_is_raw) {
    if (num_elems != new_num_elems && new_num_elems > 0) {
      uint32_t bytes_per_data_type = (uint32_t)Render::TGPUBuffer::bytesPerDataType(new_data_type);
      destroy();
      data_type = new_data_type;
      createSimple(bytes_per_data_type, new_num_elems, false, new_is_raw);
      setName(new_name);
      return true;
    }
    return false;
  }

  // -------------------------------------------------------------------------------------
  bool TGPUBuffer::createSimple(uint32_t new_bytes_per_elem, uint32_t new_num_elems, bool is_indirect, bool new_is_raw) {

    // Save input params
    is_raw = new_is_raw;
    bytes_per_elem = new_bytes_per_elem;
    num_elems = (uint32_t)new_num_elems;

    total_bytes = bytes_per_elem * num_elems;

    assert(!buffer && !srv);

    // Set flags
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = (UINT)total_bytes;

    if (is_raw) {
      bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS
        | D3D11_BIND_SHADER_RESOURCE
        ;
      bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    }
    else {
      if (is_indirect) {
        bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS
          | D3D11_BIND_SHADER_RESOURCE
          ;
        bd.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS
          | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        // All indirect buffers are raw
        is_raw = true;
      }
      else {
        bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS
          | D3D11_BIND_SHADER_RESOURCE
          ;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
      }
    }

    bd.StructureByteStride = bytes_per_elem;

    // Create the buffer in the GPU
    D3D11_SUBRESOURCE_DATA* sd_addr = nullptr;
    HRESULT hr = RenderPlatform::device->CreateBuffer(&bd, sd_addr, &buffer);
    if (FAILED(hr))
      return false;
    setDbgName(buffer, name.c_str());

    // This is to be able to use it in a shader
    if (!is_indirect && !is_raw) {
      D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
      ZeroMemory(&srv_desc, sizeof(srv_desc));
      srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
      srv_desc.BufferEx.FirstElement = 0;
      srv_desc.Format = DXGI_FORMAT_UNKNOWN;
      srv_desc.BufferEx.NumElements = num_elems;
      if (FAILED(RenderPlatform::device->CreateShaderResourceView(buffer, &srv_desc, &srv)))
        return false;
      setDbgName(srv, (name + ".srv").c_str());
    }
    else {
      D3D11_BUFFER_DESC descBuf = {};
      buffer->GetDesc(&descBuf);
      D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
      desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
      desc.BufferEx.FirstElement = 0;
      if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS)
      {
        // This is a Raw Buffer
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        desc.BufferEx.NumElements = descBuf.ByteWidth / 4;
      }
      else if (descBuf.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
      {
        // This is a Structured Buffer
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.BufferEx.NumElements = descBuf.ByteWidth / descBuf.StructureByteStride;
      }
      else
      {
        fatal("Invalid arg in srv");
      }

      if (FAILED(RenderPlatform::device->CreateShaderResourceView(buffer, &desc, &srv)))
        return false;
      setDbgName(srv, (name + ".srv").c_str());
    }

    // This is to be able to use it as a unordered access buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
    ZeroMemory(&uav_desc, sizeof(uav_desc));
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    // Format must be must be DXGI_FORMAT_UNKNOWN, when creating 
    // a View of a Structured Buffer
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.Buffer.NumElements = num_elems;
    if (is_raw) {
      uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
      uav_desc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_RAW;
      uav_desc.Buffer.NumElements = bd.ByteWidth / 4;
    }
    if (FAILED(RenderPlatform::device->CreateUnorderedAccessView(buffer, &uav_desc, &uav)))
      return false;
    setDbgName(uav, (name + ".uav").c_str());

    return true;
  }

  // -------------------------------------------------------------------------------------
  bool TGPUBuffer::create(const TBuffer& initial_data) {
    assert(!buffer && !srv);

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = total_bytes;
    bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS
      | D3D11_BIND_SHADER_RESOURCE
      ;
    //bd.Usage     = D3D11_USAGE_DYNAMIC;
    //bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (is_draw_indirect) {
      bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
      bd.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS
        | D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    }
    else
      bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    bd.StructureByteStride = bytes_per_elem;

    D3D11_SUBRESOURCE_DATA sd;
    D3D11_SUBRESOURCE_DATA* sd_addr = nullptr;
    if (!initial_data.empty()) {
      ZeroMemory(&sd, sizeof(sd));
      sd.pSysMem = initial_data.data();
      sd_addr = &sd;
    }
    HRESULT hr = RenderPlatform::device->CreateBuffer(&bd, sd_addr, &buffer);
    if (FAILED(hr))
      return false;
    setDbgName(buffer, name.c_str());

    // I might want to access a StructuredBuffer from a VS, so I need a SRV 
    // This is to be able to use it in a shader
    if (!is_draw_indirect) {
      D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
      ZeroMemory(&srv_desc, sizeof(srv_desc));
      srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
      srv_desc.BufferEx.FirstElement = 0;
      srv_desc.Format = DXGI_FORMAT_UNKNOWN;
      srv_desc.BufferEx.NumElements = num_elems;
      if (FAILED(RenderPlatform::device->CreateShaderResourceView(buffer, &srv_desc, &srv)))
        return false;
      setDbgName(srv, (name + ".srv").c_str());
    }

    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;
    ZeroMemory(&uav_desc, sizeof(uav_desc));
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    // Format must be must be DXGI_FORMAT_UNKNOWN, when creating 
    // a View of a Structured Buffer
    uav_desc.Buffer.Flags = 0;
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.Buffer.NumElements = num_elems;
    if (has_counter)
      uav_desc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_COUNTER;
    if (append)
      uav_desc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_APPEND;
    if (is_raw) {
      uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
      //uav_desc.Format = DXGI_FORMAT_UNKNOWN;
      uav_desc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_RAW;
      uav_desc.Buffer.NumElements = bd.ByteWidth / 4;
    }
    if (FAILED(RenderPlatform::device->CreateUnorderedAccessView(buffer, &uav_desc, &uav)))
      return false;
    setDbgName(uav, (name + ".uav").c_str());

    if (autocleared) {
      ZeroMemory(&bd, sizeof(bd));
      bd.Usage = D3D11_USAGE_DEFAULT;
      bd.ByteWidth = total_bytes;
      bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
      bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
      bd.StructureByteStride = bytes_per_elem;

      u8* clear_data = new u8[total_bytes];
      memset(clear_data, 0, total_bytes);
      ZeroMemory(&sd, sizeof(sd));
      sd.pSysMem = clear_data;
      hr = RenderPlatform::device->CreateBuffer(&bd, &sd, &buffer_clean);
      delete[] clear_data;
      if (FAILED(hr))
        return false;
      setDbgName(buffer_clean, (name + ".clean").c_str());
    }

    cpu_data.resize(total_bytes);

    return true;
  }

  // -------------------------------------------------------------------------------------
  bool TGPUBuffer::setMember(const char* member_name, const void* new_data, uint32_t new_bytes) {
    for (auto& m : members) {
      if (m.name != member_name)
        continue;
      void* addr = (u8*)cte.cpu_vertices.data() + m.offset;
      assert(new_bytes == bytesPerDataType(m.data_type));
      memcpy(addr, new_data, new_bytes);
      RenderPlatform::ctx->UpdateSubresource(cte.cb, 0, nullptr, cte.cpu_vertices.data(), 0, 0);
      return true;
    }
    return false;
  }

  // -------------------------------------------------------------------------------------
  void TGPUBuffer::clear(u8 new_value) {
    assert(autocleared);
    RenderPlatform::ctx->CopyResource(buffer, buffer_clean);
  }

  // -------------------------------------------------------------------------------------
  static int labels_level = 0;

  void TCmdBuffer::pushGroup(const char* label) {
    CGpuTrace::push(label);
    ++labels_level;
  }

  void TCmdBuffer::popGroup() {
    assert(labels_level > 0);
    --labels_level;
    CGpuTrace::pop();
  }

  void TCmdBuffer::deactivateTexture(int slot) {
    ID3D11ShaderResourceView* null_resource = nullptr;
    RenderPlatform::ctx->PSSetShaderResources(slot, 1, &null_resource);
  }

  void TCmdBuffer::deactivateMesh(int slot) {
    uint32_t null_stride = 0;
    uint32_t null_offset = 0;
    ID3D11Buffer* vb_null = 0;
    RenderPlatform::ctx->IASetVertexBuffers(slot, 1, &vb_null, &null_stride, &null_offset);
  }

  void TCmdBuffer::activateBuffer(const char* name, const TGPUBuffer& buffer) {
    assert(curr_pipeline);
    RenderPlatform::activateBufferInPipeline(name, buffer, curr_pipeline);
  }

  void TCmdBuffer::activateMesh(const TMesh& mesh, int slot) {
    assert(&mesh);
    assert(curr_pipeline);
    assert(mesh.vertex_decl);
    assert(curr_pipeline->vert_decl->isCompatibleWith(mesh.vertex_decl)
      || fatal("Can't render mesh %s (%s) from pipeline %s (%s)\n",
        mesh.getCName(), mesh.vertex_decl ? mesh.vertex_decl->getCName() : "null",
        curr_pipeline->getCName(), curr_pipeline->vert_decl ? curr_pipeline->vert_decl->getCName() : "null"
      )
    );
    UINT stride = mesh.vertex_decl->bytes_per_vertex;
    UINT offset = 0;

    RenderPlatform::ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY(mesh.primitive_type));

    if (mesh.nindices)
      RenderPlatform::ctx->IASetIndexBuffer(mesh.ib, mesh.bytes_per_index == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);

    if(mesh.srv) {
      if( slot == -1 )
        slot = 1;
      RenderPlatform::ctx->VSSetShaderResources( slot, 1, &mesh.srv);
    }
    else {
      if (slot == -1)
        slot = 0;
      RenderPlatform::ctx->IASetVertexBuffers(slot, 1, &mesh.vb, &stride, &offset);
    }
  }

  void TCmdBuffer::drawInstancedIndirect(const TMesh& mesh, const TGPUBuffer& buffer, uint32_t offset) {
    activateMesh(mesh);
    if (mesh.ib)
      RenderPlatform::ctx->DrawIndexedInstancedIndirect(buffer.buffer, offset);
    else
      RenderPlatform::ctx->DrawInstancedIndirect(buffer.buffer, offset);
  }

  void TCmdBuffer::drawInstancedIndirect(const ePrimitiveType prim_type, const TGPUBuffer& buffer, uint32_t offset) {
    RenderPlatform::ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY(prim_type));
    RenderPlatform::ctx->DrawInstancedIndirect(buffer.buffer, offset);
  }


  void TCmdBuffer::drawInstanced(const TMesh& mesh, uint32_t ninstances, int group_idx) {
    activateMesh(mesh);
    assert(group_idx < mesh.groups.size());
    auto& g = mesh.groups[group_idx];
    if (mesh.ib)
      RenderPlatform::ctx->DrawIndexedInstanced(g.count, ninstances, 0, g.first, 0);
    else
      RenderPlatform::ctx->DrawInstanced(g.count, ninstances, g.first, 0);
  }

  void TCmdBuffer::drawInstanced(const ePrimitiveType prim_type, uint32_t vertexs_per_instance, uint32_t ninstances) {
    RenderPlatform::ctx->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY(prim_type));
    RenderPlatform::ctx->DrawInstanced(vertexs_per_instance, ninstances, 0, 0);
  }

  void TCmdBuffer::draw(const TMesh& mesh, int group_idx) {
    if (mesh.ninstances == 1) {
      activateMesh(mesh);
      assert(group_idx < mesh.groups.size());
      auto& g = mesh.groups[group_idx];
      if (mesh.ib)
        RenderPlatform::ctx->DrawIndexed(g.count, 0, g.first);
      else
        RenderPlatform::ctx->Draw(g.count, g.first);
    }
    else if (mesh.ninstances == 0)
      return;
    else
      drawInstanced(mesh, mesh.ninstances, group_idx);
  }

  void TCmdBuffer::updateBuffer(const TGPUBuffer& buffer, const void* data) {
    assert(data);
    RenderPlatform::ctx->UpdateSubresource(buffer.buffer, 0, nullptr, data, 0, 0);
  }

  void TCmdBuffer::updateCte(const TShaderCteSingleBase& cte, const void* data) {
    RenderPlatform::ctx->UpdateSubresource(cte.cb, 0, nullptr, data, 0, 0);
  }

  void TCmdBuffer::activateCte(const TShaderCteBase& cte) {
    PROFILE_SCOPED_NAMED("Cte");
    assert(cte.cb);
    if (cte.is_dirty) {
      RenderPlatform::ctx->UpdateSubresource(cte.cb, 0, nullptr, cte.cpu_vertices.data(), 0, 0);
      auto non_cte = const_cast<const TShaderCteBase&>(cte);;
      non_cte.is_dirty = false;
    }
    RenderPlatform::ctx->VSSetConstantBuffers(cte.slot, 1, &cte.cb);
    RenderPlatform::ctx->PSSetConstantBuffers(cte.slot, 1, &cte.cb);
  }

  void TCmdBuffer::activateCteIndexed(const TShaderCteArrayBase& cte, uint32_t idx) {
    assert(cte.cb);
    assert(idx < cte.ninstances);
    RenderPlatform::ctx->UpdateSubresource(cte.cb, 0, nullptr, cte.cpu_vertices.data() + idx * cte.bytes_per_instance, 0, 0);
    RenderPlatform::ctx->VSSetConstantBuffers(cte.slot, 1, &cte.cb);
    RenderPlatform::ctx->PSSetConstantBuffers(cte.slot, 1, &cte.cb);
  }

  void TCmdBuffer::setViewport(const TViewport& vp) {
    assert(sizeof(TViewport) == sizeof(D3D11_VIEWPORT));
    RenderPlatform::ctx->RSSetViewports(1, (D3D11_VIEWPORT*)&vp);
  }

  void TCmdBuffer::setDepthState(eDepthState idx) {
    RenderPlatform::ctx->OMSetDepthStencilState(RenderPlatform::z_cfgs[(int)idx], 0);
  }

  void TCmdBuffer::setSamplerState(eSamplerState idx) {
    RenderPlatform::ctx->PSSetSamplers((int)idx, 1, &RenderPlatform::all_samplers[(int)idx]);
    RenderPlatform::ctx->CSSetSamplers((int)idx, 1, &RenderPlatform::all_samplers[(int)idx]);
  }

  void TCmdBuffer::setRasterizerState(eRSConfig cfg) {
    RenderPlatform::ctx->RSSetState(RenderPlatform::rasterize_states[(int)cfg]);
  }

  //void TCmdBuffer::setBlendState(eBlendConfig cfg) {
  //  float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };    // Not used
  //  UINT sampleMask = 0xffffffff;
  //  RenderPlatform::ctx->OMSetBlendState(RenderPlatform::blend_states[cfg], blendFactor, sampleMask);
  //}

  void TCmdBuffer::setFragmentTexture(const TTexture& texture, int slot) {
    assert(texture.isValid());
    RenderPlatform::ctx->PSSetShaderResources(slot, 1, &texture.resource_view);
  }

  const TPipelineState* TCmdBuffer::setPipelineState(const TPipelineState& pipeline) {

    if (curr_pipeline == &pipeline)
      return curr_pipeline;

    const TPipelineState* prev_pipeline = curr_pipeline;

    pipeline.vs.activate();
    pipeline.ps.activate();

    setRasterizerState(pipeline.rs_cfg);

    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };    // Not used
    UINT sampleMask = 0xffffffff;
    RenderPlatform::ctx->OMSetBlendState(RenderPlatform::blend_states[pipeline.blend_cfg], blendFactor, sampleMask);

    // Auto activate some textures needed by the technique
    for (auto& t : pipeline.textures)
      setFragmentTexture(*t.texture, t.slot);

    // Auto activate some buffers needed by the technique
    for (auto& t : pipeline.gpu_buffers) {
      if (t.buffer->cte.cb)
        activateCte(t.buffer->cte);
      else {
        assert(t.buffer->srv);
        RenderPlatform::ctx->VSSetShaderResources(t.slot, 1, &t.buffer->srv);
      }
    }

    // Set All sample states
    RenderPlatform::ctx->PSSetSamplers(0, (int)eSamplerState::COUNT, RenderPlatform::all_samplers);
    RenderPlatform::ctx->CSSetSamplers(0, (int)eSamplerState::COUNT, RenderPlatform::all_samplers);

    curr_pipeline = &pipeline;
    return prev_pipeline;
  }

  void TCmdBuffer::activateMaterial(const TMaterial& material) {
    setPipelineState(*material.pipeline);

    for (uint32_t i = 0; i < material.nslots_used; ++i) {
      const auto& m = material.slots[i];
      setFragmentTexture(*m.texture, m.slot);
    }
  }

  void TCmdBuffer::renderImGuiDrawData(ImDrawData* drawData) {
    ImGui_ImplDX11_RenderDrawData(drawData);
  }

  void debugInMenu() {
    RenderPlatform::cached_shaders.debugInMenu();
  }


}

