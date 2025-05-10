#include "platform.h"
#include <d3d11.h>
#include <d3d9.h>         // D3DPERF_*
#include <d3dcompiler.h>
#include <d3dcompiler.inl>
#include "render/render.h"

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"d3d9.lib")

#define SAFE_RELEASE(x) if( x ) x->Release(), x = nullptr

#if defined(_DEBUG) || defined(PROFILE)
#define setDbgName(obj,name) (obj)->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT) strlen( name ), name )
#else
#define setDbgName(obj,name) do { } while(false)
#endif

// https://github.com/TheRealMJP/BakingLab/blob/master/SampleFramework11/v1.02/Graphics/ShaderCompilation.cpp#L131
class FrameworkInclude : public ID3DInclude {
public:
	std::vector<const char*> paths;

	HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override {
		for (auto& path : paths) {
			std::string filepath = std::string(path) + std::string(pFileName);
			TBuffer b;
			if (!b.load(filepath.c_str()))
				continue;
			*pBytes = UINT(b.size());
			void* data = std::malloc(*pBytes);
			if (data)
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
	//TFileContext fc(strName);

	// cacheName should include file,Fn,SM,defines,timestamp of dependend files
	std::string cacheName = strName + std::string(TTempStr("_Fn_%s_SM_%s", szEntryPoint, szShaderModel));

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
	//dwShaderFlags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
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
	if (pf.path != "data/shaders/")
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
				fatal("Error compiling shader %s (%08x):\n%s\n", szFileName, hr, msg );
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

	return true;
}

namespace RenderPlatform {

	static ID3D11Device* device = nullptr;
	static ID3D11DeviceContext* ctx = nullptr;
	static IDXGISwapChain* swap_chain = nullptr;
	static ID3D11RenderTargetView* backbuffer_rtv = nullptr;
	static ID3D11Texture2D* depth_stencil = nullptr;
	static ID3D11DepthStencilView* depth_stencil_view = nullptr;
	static ID3D11Buffer* cte_buffer_offsets = nullptr;
	static ID3D11SamplerState* all_samplers[(int)Render::eSamplerState::COUNT];
	static int cpu_buffer_offsets[8] = { };
	static int xres = 0;
	static int yres = 0;

	static ID3D11RasterizerState* rasterize_states[(int)Render::eRSConfig::COUNT];
	static ID3D11BlendState* blend_states[(int)Render::eBlendConfig::COUNT];
	static ID3D11DepthStencilState* z_cfgs[(int)Render::eDepthState::COUNT];

	static Render::RenderToTexture rt_backbuffer;

	Vector< IDXGIAdapter1* > getAdapters() {
		Vector< IDXGIAdapter1* > adapters;
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

	//// --------------------------------------
	bool createBackBuffer() {
		assert(swap_chain);

		// Create a render target view
		ID3D11Texture2D* pBackBuffer = NULL;
		HRESULT hr = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
		if (FAILED(hr))
			return false;
		hr = device->CreateRenderTargetView(pBackBuffer, NULL, &backbuffer_rtv);
		pBackBuffer->Release();
		setDbgName(backbuffer_rtv, "BackBuffer");

		// Create a depth buffer
		D3D11_TEXTURE2D_DESC descDepth;
		ZeroMemory(&descDepth, sizeof(descDepth));
		descDepth.Width = xres;
		descDepth.Height = yres;
		descDepth.MipLevels = 1;
		descDepth.ArraySize = 1;
		descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDepth.SampleDesc.Count = 1;
		descDepth.SampleDesc.Quality = 0;
		descDepth.Usage = D3D11_USAGE_DEFAULT;
		descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		descDepth.CPUAccessFlags = 0;
		descDepth.MiscFlags = 0;

		hr = device->CreateTexture2D(&descDepth, NULL, &depth_stencil);
		if (FAILED(hr))
			return false;

		// Create the depth stencil view
		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		ZeroMemory(&descDSV, sizeof(descDSV));
		descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Texture2D.MipSlice = 0;
		hr = device->CreateDepthStencilView(depth_stencil, &descDSV, &depth_stencil_view);
		if (FAILED(hr))
			return false;

		setDbgName(depth_stencil_view, "MainBackBuffer.View");
		setDbgName(depth_stencil, "MainBackBuffer");

		D3D11_TEXTURE2D_DESC desc = {};
		pBackBuffer->GetDesc(&desc);
		RenderPlatform::xres = desc.Width;
		RenderPlatform::yres = desc.Height;

		// Initialize struct to define the background render target
		rt_backbuffer.width = RenderPlatform::xres;
		rt_backbuffer.height = RenderPlatform::yres;
		rt_backbuffer.render_target_view = backbuffer_rtv;
		rt_backbuffer.depth_stencil_view= depth_stencil_view;
		return true;
	}

	void destroyDevice() {
		SAFE_RELEASE(ctx);
		SAFE_RELEASE(swap_chain);
		SAFE_RELEASE(device);
	}

	bool createBufferOffsetsCtes() {
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(cpu_buffer_offsets);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		HRESULT hr = device->CreateBuffer(&bd, NULL, &cte_buffer_offsets);
		if (FAILED(hr))
			return false;
		setDbgName(cte_buffer_offsets, "cte_buffer_offsets");
		return true;
	}

	void destroyBackBuffer() {
		SAFE_RELEASE(depth_stencil_view);
		SAFE_RELEASE(depth_stencil);
		SAFE_RELEASE(backbuffer_rtv);
	}

	// -------------------------------------------------------------------
	bool createRasterizationStates() {
		rasterize_states[(int)Render::eRSConfig::DEFAULT] = nullptr;

		auto createState = [&](Render::eRSConfig cfg, const D3D11_RASTERIZER_DESC& desc) {
			const char* title = Render::rsConfigs.nameOf(cfg);
			HRESULT hr = device->CreateRasterizerState(&desc, &rasterize_states[(int)cfg]);
			if (FAILED(hr))
				return false;
			setDbgName(rasterize_states[(int)cfg], title);
			return true;
		};

		D3D11_RASTERIZER_DESC desc;

		// Depth bias options when rendering the shadows
		desc = D3D11_RASTERIZER_DESC {
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
		if (!createState(Render::eRSConfig::SHADOWS, desc))
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
		if (!createState(Render::eRSConfig::CULL_NONE, desc))
			return false;

		//desc.ScissorEnable = TRUE;
		//if (!createState(Render::eRSConfig::CULL_NONE_WITH_SCISSORS, desc))
		//	return false;

		// Culling is reversed. Used when rendering the light volumes
		//desc.CullMode = D3D11_CULL_FRONT;
		//if (!createState(Render::eRSConfig::REVERSE_CULLING, desc, "RS_REVERSE_CULLING"))
		//	return false;

		//// Wireframe and default culling back
		//desc.FillMode = D3D11_FILL_WIREFRAME;
		//desc.CullMode = D3D11_CULL_NONE;
		//if (!createState(Render::eRSConfig::WIREFRAME, desc, "RS_WIREFRAME"))
		//	return false;

		return true;
	}

	void destroyRasterizeStates() {
		for (auto c : rasterize_states)
			SAFE_RELEASE(c);
	}

	bool createBlendStates() {
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
			// PCF sampling
			ZeroMemory(&sampDesc, sizeof(sampDesc));
			sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
			sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
			sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
			sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
			sampDesc.BorderColor[0] = 1.0f;
			sampDesc.BorderColor[1] = 1.0f;
			sampDesc.BorderColor[2] = 1.0f;
			sampDesc.BorderColor[3] = 1.0f;
			if (!createState(Render::eSamplerState::PCF_SHADOWS, sampDesc, "PCF_SHADOWS"))
				return false;
		}

		//{
		//	ZeroMemory(&sampDesc, sizeof(sampDesc));
		//	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		//	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		//	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		//	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		//	sampDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		//	sampDesc.MinLOD = 0;
		//	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		//	if (!createState(Render::eSamplerState::POINT_CLAMP, sampDesc, "CLAMP_POINT"))
		//		return false;
		//}

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
	
	void setRasterizerState(Render::eRSConfig cfg) {
		ctx->RSSetState(rasterize_states[(int)cfg]);
	}

	void getBackBufferSize(int* width, int* height) {
		*width = xres;
		*height = yres;
	}

	bool resizeBackBuffer(int new_width, int new_height) {
		if( !swap_chain )
			return false;
		if( new_height == yres && new_width == xres )
			return false;
		destroyBackBuffer();
		HRESULT hr = swap_chain->ResizeBuffers(0, (UINT)new_width, (UINT)new_height, DXGI_FORMAT_UNKNOWN, 0);
		if (FAILED(hr))
			return false;
		xres = new_width;
		yres = new_height;
		if (!createBackBuffer())
			return false;
		return true;
	}

	void beginFrame(uint32_t frame_id) {
		(void) frame_id;
		static Render::Encoder encoder;
		encoder.ctx = ctx;
		Render::setMainEncoder(&encoder);
	}

	void beginRenderingBackBuffer() {
		rt_backbuffer.beginRendering();
	}

	void endRenderingBackBuffer() {
    rt_backbuffer.endRendering();
	}

	void swapFrames() {
		ID3D11RenderTargetView* null_rtv = nullptr;
		ctx->OMSetRenderTargets(1, &null_rtv, nullptr);
		assert(swap_chain);
		swap_chain->Present(0, 0);
	}

	bool create(HWND hWnd) {

		if (!createDevice(hWnd))
			return false;

		if (!createBackBuffer())
			return false;

		if (!createBufferOffsetsCtes())
			return false;

		if (!createRasterizationStates())
			return false;

		if (!createBlendStates())
			return false;

		if (!createDepthStates())
			return false;

		if (!createSamplers())
			return false;

		return true;
	}

	void destroy() {
		SAFE_RELEASE(cte_buffer_offsets);
		destroySamplers();
		destroyDepthStates();
		destroyBlendStates();
		destroyRasterizeStates();
		destroyBackBuffer();
		destroyDevice();
	}

	FORCEINLINE HRESULT
		D3D11Reflect(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
			_In_ SIZE_T SrcDataSize,
			_Out_ ID3D11ShaderReflection** ppReflector) {
		return D3DReflect(pSrcData, SrcDataSize,
			IID_ID3D11ShaderReflection, (void**)ppReflector);
	}

	// Define details of private struct
	struct PipelineState::CShaderBase::CShaderReflectionInfo {
		Vector<D3D11_SHADER_INPUT_BIND_DESC> bound_resources;
		const D3D11_SHADER_INPUT_BIND_DESC* findBindPoint(const char* name) const;
	};

	// --------------------------------------------------------------------
	bool PipelineState::CShaderBase::scanResourcesFrom(const TBuffer& blob) {
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
			sib_desc.BindCount = getID(sib_desc.Name);
			// Dupe the returned memory to keep a copy
			sib_desc.Name = _strdup(sib_desc.Name);
			reflection_info->bound_resources.push_back(sib_desc);
		}
		reflection->Release();
		return true;
	}

	// --------------------------------------------------------------------
	void PipelineState::CVertexShader::destroy() {
		SAFE_RELEASE(vs);
		SAFE_RELEASE(vtx_layout);
	}

	bool PipelineState::CVertexShader::compile(
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

		TStr128 full_name("%s@%s", szEntryPoint, szFileName );
		setDbgName(vs, full_name.c_str());

		assert(vertex_type_name);
		auto decl = Render::getVertexDeclByName(vertex_type_name);
		assert(decl || fatal("Can't identify vertex decl named '%s'\n", vertex_type_name));

		// Create the input layout
		vtx_layout = nullptr;
		//UINT nelems = (UINT)decl->num_elems;
		//const D3D11_INPUT_ELEMENT_DESC* elems = static_cast<const D3D11_INPUT_ELEMENT_DESC*>(decl->elems);
		//hr = RenderPlatform::device->CreateInputLayout(elems, nelems, VSBlob.data(),
		//	VSBlob.size(), &vtx_layout);
		//if (FAILED(hr)) {
		//	fatal("Failed CreateInputLayout(%s,%d)\n", decl->name, VSBlob.size());
		//	return false;
		//}
		//setDbgName(vtx_layout, decl->name);

		shader_src = szFileName;
		shader_fn = szEntryPoint;
		shader_profile = profile;
		shader_vtx_type_name = vertex_type_name;

		scanResourcesFrom(VSBlob);

		return true;
	}

	void PipelineState::CVertexShader::activate() const {
		if (vtx_layout)
			ctx->IASetInputLayout(vtx_layout);
		if (vs)
			ctx->VSSetShader(vs, nullptr, 0);
	}

	// --------------------------------------------------------------------
	void PipelineState::CPixelShader::destroy() {
		SAFE_RELEASE(ps);
	}

	bool PipelineState::CPixelShader::compile(
		const char* szFileName
		, const char* szEntryPoint
		, const char* profile
	) {
		if(!szEntryPoint || szEntryPoint[0] == 0x00 ) {
			ps = nullptr;
			return true;
		}

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

	void PipelineState::CPixelShader::activate() const {
		ctx->PSSetShader(ps, nullptr, 0);
	}

	// --------------------------------------------------------------------
	void RenderToTexture::destroyRT() {
		SAFE_RELEASE(render_target_view);
		SAFE_RELEASE(depth_stencil_view);
		SAFE_RELEASE(depth_srv);
	}

	void RenderToTexture::clearRT(VEC4 color) {
		assert(RenderPlatform::ctx);
		assert(render_target_view != nullptr);
		RenderPlatform::ctx->ClearRenderTargetView(render_target_view, &color.x);
	}

	void RenderToTexture::clearDepth(float clearDepthValue) {
    assert(RenderPlatform::ctx);
    RenderPlatform::ctx->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH, clearDepthValue, 0);
	}

}

namespace Render {

	using namespace RenderPlatform;

	DXGI_FORMAT asDXFormat(Render::eFormat fmt) {
		switch (fmt) {
		case eFormat::INVALID: return DXGI_FORMAT_UNKNOWN;
		case eFormat::BGRA_8UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
		case eFormat::RGBA_8UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case eFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
    case eFormat::R16_UNORM: return DXGI_FORMAT_R16_UNORM;
		case eFormat::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
		default:
			fatal("Render Format %d is not valid in DX\n", (int)fmt);
		}
		return DXGI_FORMAT_UNKNOWN;
	}


	// -------------------------------------------------------
	bool Mesh::create(
		const void* in_data,
		uint32_t in_nvertices,
		ePrimitiveType in_primitive_type,
		const void* in_index_data,
		uint32_t in_nindices,
		uint32_t in_bytes_per_index,
		const VertexDecl* in_vertex_decl
	) {

		assert(device);
		assert(in_data);
		assert(in_vertex_decl);

		nvertices = in_nvertices;
		primitive_type = in_primitive_type;
		vertex_decl = in_vertex_decl;
		size_t total_bytes = in_nvertices * vertex_decl->bytes_per_vertex;

		{
			D3D11_BUFFER_DESC bd = {};
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.ByteWidth = (UINT)total_bytes;
			bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			bd.StructureByteStride = vertex_decl->bytes_per_vertex;

			// The initial contents of the VB if something has been given
			D3D11_SUBRESOURCE_DATA InitData;
			ZeroMemory(&InitData, sizeof(InitData));
			InitData.pSysMem = in_data;
			D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
			if (in_data)
				pInitData = &InitData;

			HRESULT hr = RenderPlatform::device->CreateBuffer(&bd, pInitData, &vb);
			if (FAILED(hr))
				return false;

			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			srv_desc.Format = DXGI_FORMAT_UNKNOWN;
			srv_desc.BufferEx.NumElements = nvertices;
			hr = RenderPlatform::device->CreateShaderResourceView(vb, &srv_desc, &srv);
			if (FAILED(hr))
				return false;
		}

		// Index buffer
		if (in_index_data) {
			assert(in_nindices > 0);
			bytes_per_index = in_bytes_per_index;
			assert(bytes_per_index == 2 || bytes_per_index == 4);
			
			{
				D3D11_BUFFER_DESC bd = {};
				bd.Usage = D3D11_USAGE_DEFAULT;
				bd.ByteWidth = (UINT)in_nindices * bytes_per_index;
				bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

				D3D11_SUBRESOURCE_DATA InitData = {};
				InitData.pSysMem = in_index_data;
				D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
				if (in_index_data)
					pInitData = &InitData;
				HRESULT hr = RenderPlatform::device->CreateBuffer(&bd, pInitData, &ib);
				if (FAILED(hr))
					return false;
				nindices = in_nindices;
				assert(ib);
			}
		}

		ranges.clear();
		ranges.push_back(Range{ 0, in_index_data ? nindices : nvertices });

		return true;
	}

	void Mesh::destroy() {
		SAFE_RELEASE(ib);
		SAFE_RELEASE(srv);
		SAFE_RELEASE(vb);
	}

	void Mesh::setName(const char* new_name) {
		assert(vb);
		IResource::setName(new_name);
		setDbgName(vb, new_name);
		setDbgName(srv, new_name);
		if(ib)
			setDbgName(ib, new_name);
	}

	// -------------------------------------------------------
	bool Buffer::create(size_t total_bytes) {
		assert(device);
		assert(buffer == nullptr);

		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = (UINT)total_bytes;
		bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bd.StructureByteStride = bytes_per_instance;

		HRESULT hr = RenderPlatform::device->CreateBuffer(&bd, nullptr, &buffer);
		if (FAILED(hr))
			return false;

		cpu_buffer.resize( total_bytes );

		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srv_desc.Format = DXGI_FORMAT_UNKNOWN;
		srv_desc.BufferEx.NumElements = num_instances;
		hr = RenderPlatform::device->CreateShaderResourceView(buffer, &srv_desc, &srv);
		if (FAILED(hr))
			return false;

		return true;
	}

	void Buffer::destroy() {
		SAFE_RELEASE(srv);
		SAFE_RELEASE(buffer);
	}

	void* Buffer::rawData() {
		assert(!cpu_buffer.empty());
		dirty = true;
		assert(buffer);
		return cpu_buffer.data();
	}

	void Buffer::setName(const char* new_name) {
		assert(buffer);
		IResource::setName(new_name);
		setDbgName(buffer, new_name);
	}

	bool Texture::decodeFrom(const TBuffer& buffer) {
		ID3D11Resource* texture2d = nullptr;
		HRESULT hr = DirectX::CreateDDSTextureFromMemory(device, buffer.data(), buffer.size(), &texture2d, &srv);
		resource = texture2d;
		return SUCCEEDED(hr);
	}


	bool Texture::create(uint32_t width, uint32_t height, const void* raw_data, eFormat fmt, int num_mips) {
		
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.Format = asDXFormat(fmt);
		desc.MipLevels = num_mips;
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.SampleDesc.Count = 1;

		assert( fmt == eFormat::BGRA_8UNORM || fmt == eFormat::RGBA_8UNORM || fmt == eFormat::R16_UNORM);
		
		int bytes_per_pixel = 4;
		D3D11_SUBRESOURCE_DATA sd = {};
		sd.pSysMem = raw_data;
		if (fmt == eFormat::R16_UNORM)
			bytes_per_pixel = 2;
		sd.SysMemPitch = desc.Width * bytes_per_pixel;
		ID3D11Texture2D* texture2d = nullptr;
		HRESULT hr = RenderPlatform::device->CreateTexture2D(&desc, &sd, &texture2d);
		if (FAILED(hr))
			return false;
		resource = texture2d;

		// Create a resource view so we can use it in the shaders as input
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		srv_desc.Format = desc.Format;
		srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srv_desc.Texture2D.MipLevels = desc.MipLevels;
		hr = RenderPlatform::device->CreateShaderResourceView(resource, &srv_desc, &srv);
		if (FAILED(hr))
			return false;

		return true;
	}
	
	void Texture::destroy() {
		SAFE_RELEASE(srv);
		SAFE_RELEASE(resource);
	}

	void Texture::setName(const char* new_name) {
		IResource::setName(new_name);
		// RT with just Z-Texture do not have a srv/resource
		if(srv)
			setDbgName(srv, new_name);
		if(resource)
			setDbgName(resource, new_name);
	}

	// -------------------------------------------------------
	static void activateMesh(const Mesh* mesh) {
		assert(mesh);
		assert(mesh->primitive_type != 0);

		if( mesh->primitive_type == ePrimitiveType::eTriangles )
			ctx->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		else if (mesh->primitive_type == ePrimitiveType::eLines)
			ctx->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
		else 
			fatal( "Invalid primitive type in mesh %d\n", mesh->primitive_type);

		if (mesh->nindices)
			ctx->IASetIndexBuffer(mesh->ib, mesh->bytes_per_index == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);

		ctx->VSSetShaderResources(0, 1, &mesh->srv);
	}

	// You MUST use a RS with the ScissorEnable = true
	//void Encoder::setScissor(int x0, int y0, int w, int h) {
	//	D3D11_RECT rect;
	//	rect.left = x0;
	//	rect.top = y0;
	//	rect.right = x0 + w;
	//	rect.bottom = y0 + h;
	//	ctx->RSSetScissorRects(1, &rect);
	//}

	void Encoder::drawMesh(const Mesh* mesh, uint32_t submesh ) {
		activateMesh( mesh );
		assert(submesh < mesh->ranges.size());
		const auto& g = mesh->ranges[submesh];
		if (mesh->ib)
			ctx->DrawIndexed(g.count, 0, g.first);
		else
			ctx->Draw(g.count, g.first);
	}

	void Encoder::drawInstancedMesh(const Mesh* mesh, int ninstances, uint32_t submesh) {
		activateMesh(mesh);
		assert(submesh < mesh->ranges.size());
		const auto& g = mesh->ranges[submesh];
		if( mesh->ib )
			ctx->DrawIndexedInstanced( g.count, ninstances, 0, g.first, 0 );
		else
			ctx->DrawInstanced(g.count, ninstances, g.first, 0);
	}

	void Encoder::setRenderPipelineState(const PipelineState* in_pipeline) {
		assert(this->ctx == RenderPlatform::ctx);
		assert(in_pipeline);
		in_pipeline->vs.activate();
		in_pipeline->ps.activate();
		ctx->VSSetConstantBuffers(0, 1, &cte_buffer_offsets);
		ctx->PSSetConstantBuffers(0, 1, &cte_buffer_offsets);
		setRasterizerState(in_pipeline->rs_cfg);

		this->ctx->OMSetDepthStencilState(RenderPlatform::z_cfgs[(int)in_pipeline->depth_state], 0);

		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };    // Not used
		UINT sampleMask = 0xffffffff;
		this->ctx->OMSetBlendState(RenderPlatform::blend_states[(int)in_pipeline->blend_cfg], blendFactor, sampleMask);

		this->ctx->PSSetSamplers(0, (int)eSamplerState::COUNT, RenderPlatform::all_samplers);

		// Activate predefined textures
		int idx = 0;
		for (auto t : in_pipeline->textures) {
			if (t)
				setTexture(t, idx);
			idx++;
		}
		// Activate predefined buffers
		for (auto b : in_pipeline->buffers) {
			assert( b );
			setVertexBuffer(b);
		}
	}

	void Encoder::setVertexBuffer(const Buffer* in_buffer) {
		assert(in_buffer->buffer);
		assert(in_buffer->srv);
		if (in_buffer->dirty) {
			RenderPlatform::ctx->UpdateSubresource(in_buffer->buffer, 0, nullptr, in_buffer->cpu_buffer.data(), 0, 0);
			auto non_cte = const_cast<Buffer*>(in_buffer);;
			non_cte->dirty = false;
		}
		ctx->VSSetShaderResources(in_buffer->slot, 1, &in_buffer->srv);
		ctx->PSSetShaderResources(in_buffer->slot, 1, &in_buffer->srv);
	}

	void Encoder::setTexture(const Texture* texture, int slot) {
		assert( texture && texture->srv );
		ctx->PSSetShaderResources(slot, 1, &texture->srv);
	}

	void Encoder::setVertexBufferOffset(int instance_idx, int bytes_per_instance, int slot) {
		assert( slot >= 0 && slot < 8 );
		(void) bytes_per_instance;
		cpu_buffer_offsets[ slot ] = instance_idx;
		ctx->UpdateSubresource(cte_buffer_offsets, 0, nullptr, cpu_buffer_offsets, 0, 0);
	}

	void Encoder::pushGroup(const char* label) {
		wchar_t wname[64];
		mbstowcs(wname, label, 64);
		D3DPERF_BeginEvent(0xffffffff, wname);
	}

	void Encoder::popGroup() {
		D3DPERF_EndEvent();
	}

	void Encoder::setBufferContents(Buffer* buffer, const void* new_data, size_t total_bytes) {
		assert( buffer );

		if (total_bytes < buffer->size()) {
			// updateBuffer will read block_size_data from data, to avoid 
			// reading from uninitialized memory
			memcpy(buffer->cpu_buffer.data(), new_data, total_bytes);
			new_data = buffer->cpu_buffer.data();
		}
		setVertexBuffer(buffer);
		setVertexBufferOffset(0, buffer->bytes_per_instance, buffer->slot);
		ctx->UpdateSubresource( buffer->buffer, 0, nullptr, new_data, 0, 0 );
	}

	bool PipelineState::create(const json& j) {
		const char* src = j.value("src", "");
		const char* in_name = j.value("name", "");

		TStr256 full_name("data/shaders/%s.hlsl", src);

		// To control which pso should be renderer before others in the render manager
		priority = j.value( "priority", 0 );

		vs.shader_fn = j.value("vs", "VS");
		if( !j["ps"].is_null() ) 
			ps.shader_fn = j.value("ps", "PS");
		vs.shader_src = full_name;
		ps.shader_src = full_name;
		const char* vertex_decl_name = j.value("vertex_decl", (const char*) nullptr);
		assert(vertex_decl_name);
		vertex_decl = getVertexDeclByName(vertex_decl_name);

		vs.shader_profile = j.value("vs_profile", "vs_5_0");
		ps.shader_profile = j.value("ps_profile", "ps_5_0");
		if( !reloadShaders() )
			return false;

		return loadCommonConfig(j);
	}

	bool PipelineState::reloadShaders() {
		PipelineState prev_pipeline = *this;
		if (!vs.compile(vs.shader_src.c_str(), vs.shader_fn.c_str(), vs.shader_profile.c_str(), vertex_decl->name))
			return false;
		if (!ps.compile(ps.shader_src.c_str(), ps.shader_fn.c_str(), ps.shader_profile.c_str()))
			return false;
		prev_pipeline.destroy();
		return true;
	}

	void PipelineState::setName(const char* new_name) {
		IResource::setName(new_name);
	}

	void PipelineState::destroy() {
		ps.destroy();
		vs.destroy();
	}

	// ------------------------------------
	bool createDepthBuffer(int width, int height, DXGI_FORMAT in_fmt,
		ID3D11DepthStencilView** depth_stencil_view_ptr,
		ID3D11ShaderResourceView** shader_resource_view_ptr,
		ID3D11Texture2D** depth_stencil
	) {
		PROFILE_SCOPED_NAMED("createDepthBuffer");
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
    else if (in_fmt == DXGI_FORMAT_R16_UNORM) {
      srv_fmt = DXGI_FORMAT_R16_UNORM;
      res_fmt = DXGI_FORMAT_R16_TYPELESS;
      dsv_fmt = DXGI_FORMAT_D16_UNORM;
    }
		else {
			fatal("Invalid depth format for depth buffer\n");
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

		HRESULT hr = device->CreateTexture2D(&descDepth, NULL, depth_stencil);
		if (FAILED(hr))
			return false;

		// Create the depth stencil view
		D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
		ZeroMemory(&descDSV, sizeof(descDSV));
		descDSV.Format = dsv_fmt;
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		descDSV.Texture2D.MipSlice = 0;
		hr = device->CreateDepthStencilView(*depth_stencil, &descDSV, depth_stencil_view_ptr);
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
			hr = device->CreateShaderResourceView(*depth_stencil, &shaderResourceViewDesc, shader_resource_view_ptr);
			if (FAILED(hr))
				return false;
		}

		return true;
	}

	// ---------------------------------------------------------------
	bool RenderToTexture::createRT(
		const char* rt_name,
		int new_xres, int new_yres,
		eFormat color_fmt, eFormat depth_fmt
	) {

		//if (color_fmt != eFormat::INVALID) {
		//	if (!Texture::create(new_xres, new_yres, color_fmt, 1, (int)eOptions::RENDER_TARGET))
		//		return false;
		//	setName("RT_" + std::string(rt_name));

		//	// Create the render target view
		//	HRESULT hr = RenderPlatform::device->CreateRenderTargetView(resource, NULL, &render_target_view);
		//	if (FAILED(hr))
		//		return false;
		//	setDbgName(render_target_view, getName());
		//}

		width = new_xres;
		height = new_yres;

		if (depth_fmt != eFormat::INVALID) {

			// Expose depth_srv as a independent texture
			if (!depth_texture)
				depth_texture = new Texture;
			else
				depth_texture->destroy();

			ID3D11Texture2D* texture2d = nullptr;
			if (!createDepthBuffer(new_xres, new_yres, asDXFormat(depth_fmt), &depth_stencil_view, &depth_srv, &texture2d))
				return false;
			depth_texture->resource = texture2d;
			setDbgName(depth_stencil_view, (std::string("Z_") + std::string(rt_name)).c_str());
			setDbgName(depth_srv, (std::string("Zsrv_") + std::string(rt_name)).c_str());

			depth_texture->srv = depth_srv;
			depth_texture->srv->AddRef();
			std::string z_new_name = std::string(rt_name) + "_z.texture";
			if (depth_texture->getName() != z_new_name) {
				depth_texture->setName(z_new_name.c_str());
				addResource(depth_texture);
			}
		}

		// We register ourselves so we can be destroyed by the resources manager
		std::string new_name = std::string(rt_name) + ".texture";
		if (name != new_name.c_str()) {
			setName( new_name.c_str() );
			addResource((Texture*)this);
		}

		return true;
	}

	void RenderToTexture::destroy() {
		RenderToTexture::destroyRT();
		Texture::destroy();
  }

  void RenderToTexture::beginRendering() {
    Render::Encoder* encoder = Render::getMainEncoder();
    assert(encoder);
    encoder->pushGroup(getName());

    ID3D11ShaderResourceView* srv_nulls[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    RenderPlatform::ctx->PSSetShaderResources(0, 6, srv_nulls);
    RenderPlatform::ctx->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    ctx->RSSetViewports(1, &vp);
  
		if( render_target_view )
			clearRT(Colors::Black);
		if( depth_stencil_view )
			clearDepth(1.0f);
  }

  void RenderToTexture::endRendering() {
    Render::Encoder* encoder = Render::getMainEncoder();
    assert(encoder);
		encoder->popGroup();
  }

}