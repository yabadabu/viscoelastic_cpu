#pragma once

// Forward reference's of pointer for DX
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11Resource;
struct ID3D11Texture2D;
struct ID3D11Texture3D;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11InputLayout;

struct HWND__;
typedef struct HWND__* HWND;

namespace RenderPlatform {

	enum eCullMode {
		eDefault
	};

	struct VertexDecl {
		void* elems = nullptr;
		size_t num_elems = 0;
	};

	struct Mesh {
		ID3D11Buffer* vb = nullptr;
		ID3D11ShaderResourceView* srv = nullptr;
		ID3D11Buffer* ib = nullptr;
	};

	struct PipelineState {

	private:
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
			ID3D11InputLayout* vtx_layout = nullptr;
			std::string        shader_vtx_type_name;
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

	public:

		CVertexShader         vs;
		CPixelShader          ps;

	};

	struct Buffer {
		ID3D11Buffer*             buffer = nullptr;
		ID3D11ShaderResourceView* srv = nullptr;
		TBuffer                   cpu_buffer;
		bool                      dirty = false;
	};

	struct Texture {
		ID3D11Resource*           resource = nullptr;
		ID3D11ShaderResourceView* srv = nullptr;
	};

	// -----------------------------------------
	struct RenderToTexture {
		ID3D11RenderTargetView*   render_target_view = nullptr;
		ID3D11DepthStencilView*   depth_stencil_view = nullptr;
		ID3D11ShaderResourceView* depth_srv = nullptr;
		void destroyRT();
		void clearRT(VEC4 color);
		void clearDepth(float clearDepthValue = 1.0f);
	};

	struct Encoder {
		ID3D11DeviceContext* ctx = nullptr;
	};

	bool create( HWND hWnd );
	void destroy();
	bool resizeBackBuffer(int new_width, int new_height);
	void getBackBufferSize( int* width, int* height );
	void beginFrame( uint32_t frame_id );
	void swapFrames();
  void beginRenderingBackBuffer();
  void endRenderingBackBuffer();

}

