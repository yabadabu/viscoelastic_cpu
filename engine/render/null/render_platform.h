#pragma once


namespace RenderPlatform {

  enum eCullMode {
    eDefault
  };

  struct VertexDecl {
    void* elems = nullptr;
    size_t num_elems = 0;
  };

  struct Mesh {
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
      bool isValid() const { return true; }
    };

    // -----------------------------------------
    class CPixelShader : public CShaderBase {
    public:
      void destroy();
      bool compile(
        const char* szFileName
        , const char* szEntryPoint
        , const char* profile
      );
      void activate() const;
      bool isValid() const { return true; }
    };

  public:

    CVertexShader         vs;
    CPixelShader          ps;

  };

  struct Buffer {
    TBuffer                   cpu_buffer;
    bool                      dirty = false;
  };

  struct Texture {
  };

  // -----------------------------------------
  struct RenderToTexture {
    void destroyRT();
    void clearRT(VEC4 color);
    void clearDepth(float clearDepthValue = 1.0f);
  };

  struct Encoder {
  };

  //bool create(HWND hWnd);
  void destroy();
  bool resizeBackBuffer(int new_width, int new_height);
  void getBackBufferSize(int* width, int* height);
  void beginFrame(uint32_t frame_id);
  void swapFrames();
  void beginRenderingBackBuffer();
  void endRenderingBackBuffer();

}

