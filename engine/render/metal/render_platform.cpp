#include "platform.h"
#include "render/render.h"
//#include "formats/ktx/ktx.h"
//#include "memory/data_provider.h"

static MTL::Device*       device = nullptr;
static MTL::CommandQueue* commandQueue = nullptr;
static MTL::DepthStencilState* zcfgs[ (int)Render::eDepthState::COUNT ];
static MTL::RenderPassDescriptor* backbuffer_passDescriptor = nullptr;
static CA::MetalDrawable* backbuffer_drawable = nullptr;
static MTL::CommandBuffer* backbuffer_commandBuffer = nullptr;
//static MTL::SamplerState* sampler_states[ (int)Render::eSamplerState::COUNT ];



#define SAFE_RELEASE(x) x->release(), x = nullptr;
#define setDbgName(obj, new_name) obj->setLabel( NS::String::string( new_name, NS::UTF8StringEncoding ) )

namespace RenderPlatform {

  MTL::Device* getDevice() { return device; }

  static uint32_t current_frame_id = 0;
  
  void beginFrame( uint32_t frame_id
                 , MTL::RenderPassDescriptor* passDescriptor
                 , MTL::CommandBuffer* new_commandBuffer
                 , CA::MetalDrawable* drawable 
  ) {
    current_frame_id = frame_id;
    backbuffer_passDescriptor = passDescriptor;
    backbuffer_commandBuffer = new_commandBuffer;
    backbuffer_drawable = drawable;
  }

  void beginRenderingBackBuffer() {
    assert( backbuffer_passDescriptor != nullptr );
    assert( backbuffer_drawable != nullptr );

    float g = 0.1f;
    MTL::ClearColor clearColor = MTL::ClearColor::Make(g,g,g, 1.0);
    MTL::RenderPassColorAttachmentDescriptor* colorDescriptor = backbuffer_passDescriptor->colorAttachments()->object(0);
    assert( colorDescriptor );
    colorDescriptor->setClearColor( clearColor );
    colorDescriptor->setLoadAction( MTL::LoadActionClear );
    colorDescriptor->setStoreAction( MTL::StoreActionStore );
    colorDescriptor->setTexture( backbuffer_drawable->texture() );

    backbuffer_commandBuffer = RenderPlatform::getCommandBuffer();
    assert( backbuffer_commandBuffer );
    static Render::Encoder encoder;
    encoder.encoder = backbuffer_commandBuffer->renderCommandEncoder( backbuffer_passDescriptor );
    assert( encoder.encoder );
    Render::setMainEncoder( &encoder );
  }

  void endRenderingBackBuffer() {
  }

  void swapFrames() {
    assert( backbuffer_passDescriptor != nullptr );
    assert( backbuffer_drawable != nullptr );

    Encoder* encoder = Render::getMainEncoder( );
    encoder->encoder->endEncoding();  
    backbuffer_commandBuffer->presentDrawable(backbuffer_drawable);
    backbuffer_commandBuffer->commit();
    Render::setMainEncoder( nullptr );

    backbuffer_drawable = nullptr;
    backbuffer_passDescriptor = nullptr;
    backbuffer_commandBuffer = nullptr;
  }

  void saveZCfg( Render::eDepthState ds_id, MTL::DepthStencilDescriptor* desc ) {
    //setDbgName( desc, Render::depthStates.nameOf( ds_id ) );
    zcfgs[ (int)ds_id ] = device->newDepthStencilState( desc );
  }

  bool createZCfgs( ) {
    MTL::DepthStencilDescriptor* desc = MTL::DepthStencilDescriptor::alloc()->init();

    desc->setDepthCompareFunction( MTL::CompareFunction::CompareFunctionLess );
    desc->setDepthWriteEnabled( true );
    saveZCfg( Render::eDepthState::LESS, desc );

    desc->setDepthCompareFunction( MTL::CompareFunction::CompareFunctionAlways );
    desc->setDepthWriteEnabled( false );
    saveZCfg( Render::eDepthState::DISABLE_ALL, desc );

    desc->setDepthCompareFunction( MTL::CompareFunction::CompareFunctionLess );
    desc->setDepthWriteEnabled( false );
    saveZCfg( Render::eDepthState::TEST_BUT_NO_WRITE, desc );

    SAFE_RELEASE( desc );
    return true;
  }

  // -------------------------------------------------------------------------------------
  void saveSampler( Render::eSamplerState id, MTL::SamplerDescriptor* desc ) {
    //setDbgName( desc, Render::eSamplerState.nameOf( id ) );
    //sampler_states[ (int)id ] = device->newSamplerState( desc );
    SAFE_RELEASE( desc );
  }

  bool createSamplers() {
    MTL::SamplerDescriptor* desc = MTL::SamplerDescriptor::alloc()->init();
    desc->setMinFilter( MTL::SamplerMinMagFilterNearest );
    desc->setMagFilter( MTL::SamplerMinMagFilterLinear );
    desc->setMipFilter( MTL::SamplerMipFilterLinear );
    desc->setSAddressMode( MTL::SamplerAddressModeRepeat );
    desc->setTAddressMode( MTL::SamplerAddressModeRepeat );
    desc->setRAddressMode( MTL::SamplerAddressModeRepeat );
    saveSampler( Render::eSamplerState::LINEAR_WRAP, desc );

    /*
    desc->setMinFilter( MTL::SamplerMinMagFilterNearest );
    desc->setMagFilter( MTL::SamplerMinMagFilterLinear );
    desc->setMipFilter( MTL::SamplerMipFilterLinear );
    desc->setSAddressMode( MTL::SamplerAddressModeRepeat );
    desc->setTAddressMode( MTL::SamplerAddressModeRepeat );
    desc->setRAddressMode( MTL::SamplerAddressModeRepeat );
    
    cconstexpr sampler shadow_sampler(coord::normalized, filter::linear, address::clamp_to_edge, compare_func::less);
    desc = [[MTL::SamplerDescriptor alloc] init];
    desc.normalizedCoordinates = true;
    desc.minFilter = MTLSamplerMinMagFilterNearest;
    desc.magFilter = MTLSamplerMinMagFilterLinear;
    desc.mipFilter = MTLSamplerMipFilterLinear;
    desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    desc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    desc.compareFunction = MTLCompareFunctionLess;
    saveSampler( Render::eSamplerState::LINEAR_WRAP, desc );
    */
    return true;
  }

  bool create( ) {
    
    device = MTL::CreateSystemDefaultDevice();
    if( !device )
      return false;

    commandQueue =  device->newCommandQueue();
    if( !commandQueue ) 
      return false;

    if( !createZCfgs() )
      return false;

    // if( !createSamplers() )
    //   return false;

    return true;
	}

  void destroy() {
    // for( auto ss : sampler_states )
    //   SAFE_RELEASE(ss);
    for( auto dss : zcfgs )
      SAFE_RELEASE(dss);
    SAFE_RELEASE(commandQueue);
    SAFE_RELEASE(device);
  }

  MTL::CommandBuffer* getCommandBuffer() {
    assert( commandQueue );
    return commandQueue->commandBuffer();
  }

  // --------------------------------------------------------------------
  void RenderToTexture::destroyRT() {
  }

  void RenderToTexture::clearRT(VEC4 color) {
  }

  void RenderToTexture::clearDepth(float clearDepthValue) {
  }

}

namespace Render {

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

    assert( device );
    assert( in_data );
    assert( in_vertex_decl );
    assert( vb == nullptr );
    assert( ib == nullptr );

    nvertices = in_nvertices;
    primitive_type = in_primitive_type;
    vertex_decl = in_vertex_decl;
    size_t total_bytes = in_nvertices * vertex_decl->bytes_per_vertex;

    vb = device->newBuffer(in_data, total_bytes, MTL::CPUCacheModeDefaultCache);
    assert( vb );

    if( in_index_data ) {
      assert( in_nindices > 0 );
      bytes_per_index = in_bytes_per_index;
      assert( bytes_per_index == 2 || bytes_per_index == 4 );
      ib = device->newBuffer(in_index_data, in_nindices * bytes_per_index, MTL::CPUCacheModeDefaultCache);
      nindices = in_nindices;
      assert( ib );
    }

    ranges.push_back( Range{ 0, in_index_data ? nindices : nvertices });

    return true;
  }

  void Mesh::destroy() {
    ranges.clear();
    SAFE_RELEASE( ib );
    SAFE_RELEASE( vb );
  }
  
  void Mesh::setName( const char* new_name ) {
    assert( vb ); 
    IResource::setName( new_name );
    vb->setLabel( NS::String::string( new_name, NS::UTF8StringEncoding ) );
  }

  // -------------------------------------------------------
  bool Buffer::create( size_t total_bytes ) {
    assert( device );
    buffer = device->newBuffer( total_bytes, MTL::ResourceStorageModeShared);
    assert( buffer );
    return true;
  }

  void Buffer::destroy() {
    SAFE_RELEASE( buffer );
  }

  void* Buffer::rawData() {
    assert( buffer );
    return buffer->contents();
  }
  
  void Buffer::setName( const char* new_name ) {
    assert( buffer ); 
    IResource::setName( new_name );
    buffer->setLabel( NS::String::string( new_name, NS::UTF8StringEncoding ) );
  }

  // -------------------------------------------------------
  static MTL::PixelFormat asMetalFormat(Render::eFormat fmt) {
    switch (fmt) {
    case eFormat::BGRA_8UNORM: return MTL::PixelFormat::PixelFormatBGRA8Unorm;
    case eFormat::RGBA_8UNORM: return MTL::PixelFormat::PixelFormatRGBA8Unorm;
    case eFormat::R8_UNORM: return MTL::PixelFormat::PixelFormatR8Unorm;
    case eFormat::R16_UNORM: return MTL::PixelFormat::PixelFormatR16Unorm;
    case eFormat::R32_FLOAT: return MTL::PixelFormat::PixelFormatR32Float;
    default:
      fatal("Render Format %d is not valid in Metal\n", (int)fmt);
    }
    return MTL::PixelFormat::PixelFormatInvalid;
  }

  bool Texture::decodeFrom(const TBuffer& b) {
    return false;
    /*
    KTXFile ktx( b.data(), b.size() );
    const KTXFile::Header& header = ktx.getHeader();
    if( !header.isValid() )
      return false;

    // Print some properties from the header
    if( 0 ) {
      dbg("Endianess: %08x\n", header.endianness);
      dbg("Width: %u\n", header.pixelWidth);
      dbg("Height: %u\n", header.pixelHeight);
      dbg("Depth: %u\n", header.pixelDepth);        // Textures 2d => 0
      dbg("Mipmap Levels: %u\n", header.numberOfMipmapLevels);
      dbg("Array Elements: %u\n", header.numberOfArrayElements);    // In non-array textures, 0
      dbg("Faces: %u\n", header.numberOfFaces);   // For cubemaps : 6
      dbg("glType: %08x\n", header.glType);
      dbg("glTypeSize: %08x\n", header.glTypeSize);
      dbg("glFormat: %08x\n", header.glFormat);       // Mus tbe 0 for compressed formats
      dbg("glInternalFormat: %08x\n", header.glInternalFormat);
      dbg("glBaseInternalFormat: %08x\n", header.glBaseInternalFormat);
      dbg("bytesOfKeyValueData: %d\n", header.bytesOfKeyValueData );
    }

    MTL::PixelFormat fmt = MTL::PixelFormatInvalid;
    // IOS
    if( header.glInternalFormat == 0x93d0)    // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR sRGB?
      fmt = MTL::PixelFormatASTC_4x4_LDR;
    else if( header.glInternalFormat == 0x8c4f) // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT (BC3)
      fmt = MTL::PixelFormatBC3_RGBA;
    else if( header.glInternalFormat == 0x8e8d) // GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM    (BC7)
      fmt = MTL::PixelFormatBC7_RGBAUnorm;
    else {
      // https://registry.khronos.org/OpenGL/api/GL/glext.h
      fatal( "Unsupported KTX format %08x\n", header.glInternalFormat);
      return false;
    }

    // ktx.onEachMetaData( []( int idx, const char* key, const char* value ) {
    //   dbg( "[%d] %s : %s\n", idx, key, value );
    // });
    assert( header.pixelDepth == 0 );

    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType( MTL::TextureType2D );
    desc->setPixelFormat( fmt );
    desc->setWidth( header.pixelWidth );
    desc->setHeight( header.pixelHeight );
    desc->setDepth( 1 );
    desc->setMipmapLevelCount( header.numberOfMipmapLevels );
    desc->setArrayLength( 1 );
    desc->setUsage( MTL::TextureUsageShaderRead );
    texture = device->newTexture( desc );

    ktx.onEachMipMap([=]( int level, const void* img_data, int img_data_size, int w, int h, int bytes_per_row ) {
      texture->replaceRegion( MTL::Region( 0, 0, w, h )
                           , NS::UInteger( level )
                           , img_data
                           , NS::UInteger( bytes_per_row ));
    });
    return true;
    */
  }

  bool Texture::create(uint32_t width, uint32_t height, const void* raw_data, eFormat fmt, int num_mips) {
    
    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType( MTL::TextureType2D );
    desc->setWidth( width );
    desc->setHeight( height );
    desc->setDepth( 1 );
    desc->setPixelFormat( asMetalFormat( fmt ) );
    desc->setArrayLength( 1 );
    desc->setMipmapLevelCount( num_mips );
    desc->setUsage( MTL::TextureUsageShaderRead );
   
    texture = device->newTexture( desc );

    int bytes_per_row = width * 4;
    texture->replaceRegion( MTL::Region( 0, 0, width, height )
                          , NS::UInteger( 0 ) // level
                          , raw_data
                          , NS::UInteger( bytes_per_row ));

    return true;
  }
  
  void Texture::destroy() {
    SAFE_RELEASE( texture );
  }

  void Texture::setName(const char* new_name) {
    IResource::setName( new_name );
    if( texture )
      texture->setLabel( NS::String::string( new_name, NS::UTF8StringEncoding ) );
  }

  // -------------------------------------------------------
  void Encoder::drawMesh( const Mesh* mesh, uint32_t submesh ) {
    encoder->setVertexBuffer( mesh->vb, 0, 0 );    // VertexData slot buffer

    assert( mesh );
    assert( submesh < mesh->ranges.size() || fatal( "mesh %s. Invalid submesh %d/%d\n", mesh->getName(), submesh, (int)mesh->ranges.size() ));
    const Mesh::Range& r = mesh->ranges[ submesh ];

    if( mesh->ib ) {
      assert( submesh == 0 ); // Until we can confirm it works.

      encoder->drawIndexedPrimitives( 
        MTL::PrimitiveType(mesh->primitive_type), 
        NS::UInteger(mesh->nindices), 
        mesh->bytes_per_index == 2 ? MTL::IndexType::IndexTypeUInt16 : MTL::IndexType::IndexTypeUInt32,
        mesh->ib, 
        NS::UInteger(0)
        );

    } else {
      encoder->drawPrimitives( MTL::PrimitiveType(mesh->primitive_type), NS::UInteger(r.first), NS::UInteger(r.count));

    }
  }

  void Encoder::setBufferContents( Buffer* buffer, const void* new_data, size_t new_data_size ) {

    // First time in this frame, we can rewind the pointer (or use a second version of the buffer)
    if( buffer->current_frame != RenderPlatform::current_frame_id ) {
      buffer->current_frame = RenderPlatform::current_frame_id;
      buffer->current_offset = 0;
    }

    size_t new_size = buffer->current_offset + new_data_size;
    if( new_size > buffer->size() ) {
      buffer->num_instances = (int)(new_size / buffer->bytes_per_instance) + 256;
      dbg( "At frame %d, no space left in Render::Buffer for new %ld bytes of data. Current usage is %d / %d (%d instances)\n", RenderPlatform::current_frame_id, new_data_size, buffer->current_offset, buffer->size(), buffer->num_instances );
      new_size = buffer->num_instances * buffer->bytes_per_instance;
      MTL::Buffer* prev_buffer = buffer->buffer;
      buffer->create( new_size );
      // Copy the current used contents
      memcpy( buffer->rawData(), prev_buffer->contents(), buffer->current_offset);
      SAFE_RELEASE( prev_buffer );
    }
    uint32_t curr_offset = buffer->current_offset;
    uint32_t instance_idx = curr_offset / buffer->bytes_per_instance;
    assert( buffer );
    memcpy( (u8*)buffer->rawData() + curr_offset, new_data, new_data_size );
    setVertexBuffer( buffer );
    setVertexBufferOffset( instance_idx, buffer->bytes_per_instance, buffer->slot );
    buffer->current_offset += new_data_size;
  }

  void Encoder::drawInstancedMesh( const Mesh* mesh, int ninstances, uint32_t submesh ) {
    encoder->setVertexBuffer( mesh->vb, 0, 0 ); 

    assert( mesh );
    assert( submesh < mesh->ranges.size() || fatal( "mesh %s. Invalid submesh %d/%d\n", mesh->getName(), submesh, (int)mesh->ranges.size() ));
    const Mesh::Range& r = mesh->ranges[ submesh ];

    if( mesh->ib ) {
      assert( submesh == 0 ); // Until we can confirm it works.

      encoder->drawIndexedPrimitives( 
        MTL::PrimitiveType(mesh->primitive_type), 
        NS::UInteger(mesh->nindices), 
        mesh->bytes_per_index == 2 ? MTL::IndexType::IndexTypeUInt16 : MTL::IndexType::IndexTypeUInt32,
        mesh->ib, 
        NS::UInteger(0),
        NS::UInteger(ninstances)
        );

    } else {

      encoder->drawPrimitives( MTL::PrimitiveType(mesh->primitive_type), 
                               NS::UInteger(r.first), 
                               NS::UInteger(r.count),
                               NS::UInteger(ninstances)
                              );
    }
  }

  void Encoder::setRenderPipelineState( const PipelineState* in_pipeline ) {
    encoder->setRenderPipelineState( in_pipeline->pipeline_state );

    // Activate the associated cullmode
    if( in_pipeline->rs_cfg == eRSConfig::CULL_NONE)
      encoder->setCullMode( MTL::CullMode::CullModeNone );
    else if( in_pipeline->rs_cfg == eRSConfig::DEFAULT)
      encoder->setCullMode( MTL::CullMode::CullModeBack );
    else if( in_pipeline->rs_cfg == eRSConfig::SHADOWS) {
      encoder->setCullMode( MTL::CullMode::CullModeBack );
      // Magic values from the apple sample
      encoder->setDepthBias( 0.015f, 7, 0.02f );
    }
    else
      fatal( "Invalid cullmode %d for pipeline %s\n", in_pipeline->rs_cfg, in_pipeline->getName() );

    encoder->setDepthStencilState( zcfgs[ (int)in_pipeline->depth_state ] );
    //encoder->setFragmentSamplerState( sampler_states[ 0 ], 0 );

    // Activate predefined textures
    int idx = 0;
    for (auto t : in_pipeline->textures) {
    	if (t)
    		setTexture(t, idx);
    	idx++;
    }

    for (auto b : in_pipeline->buffers)
      setVertexBuffer(b);

  }
  
  void Encoder::setVertexBuffer( const Buffer* in_buffer ) {
    encoder->setVertexBuffer( in_buffer->buffer, 0, in_buffer->slot );
    encoder->setFragmentBuffer( in_buffer->buffer, 0, in_buffer->slot );
  }
  
  void Encoder::setVertexBufferOffset( int instance_idx, int bytes_per_instance, int slot ) {
    encoder->setVertexBufferOffset( instance_idx * bytes_per_instance, slot );
  }
  
  void Encoder::setTexture( const Texture* texture, int slot ) {
    assert( texture );
    encoder->setFragmentTexture( texture->texture, slot );
  }

  void Encoder::pushGroup( const char* label ) {
    assert( encoder );
    encoder->pushDebugGroup( NS::String::string( label, NS::UTF8StringEncoding ) );
  }
  
  void Encoder::popGroup() {
    assert( encoder );
    encoder->popDebugGroup();
  }

  void resolveIncludes( TBuffer& buf ) {
    while( true ) {
      const char* p = (const char*) buf.data();
      const char* inc = strstr( p, "#include \"" );
      if( !inc ) 
        break;

      const char* inc_end = strchr( inc, '\n' );
      
      char include_name[256];
      const char* first_quote = strchr( inc, '"' ) + 1;
      const char* end_quote = strchr( first_quote, '"' );
      size_t name_sz = end_quote - first_quote;
      memcpy( include_name, first_quote, name_sz );
      include_name[name_sz] = 0x00;

      TBuffer inc_contents;
      if( !inc_contents.load( include_name ))
        fatal( "Failed to resolve include '%s'", include_name );

      TBuffer new_buf;
      new_buf.insert( new_buf.end(), buf.data(), (unsigned char*)inc );
      new_buf.insert( new_buf.end(), inc_contents.data(), inc_contents.data() + inc_contents.size() );
      size_t num_bytes_after = buf.size() - (inc_end - p );
      new_buf.insert( new_buf.end(), (unsigned char*)inc_end, (unsigned char*)inc_end + num_bytes_after);
      new_buf.push_back( 0x00 );
      buf = new_buf;
    }
  }

  // -------------------------------------------------------
  bool PipelineState::create( const json& j ) {
    src = j.value( "src", "" );

    // To control which pso should be renderer before others in the render manager
    priority = j.value( "priority", 0 );
    vsFn = j.value( "vs", "VS" );
    if( !j["ps"].is_null() ) 
      psFn = j.value( "ps", "PS" );
    const char* vertex_decl_name = j.value( "vertex_decl", (const char*) nullptr );
    assert( vertex_decl_name );
    vertex_decl = getVertexDeclByName( vertex_decl_name );

    if( !loadCommonConfig( j ) )
      return false;

    return reloadShaders();
  }

  bool PipelineState::reloadShaders() {
    destroy();

    std::string full_name( "data/shaders/" );
    full_name += src;
    full_name += ".metal";

    TBuffer buf;
    if (!buf.load(full_name.c_str())) {
        fatal("Failed to load pso source %s\n", full_name.c_str());
        return false;
    }
    buf.push_back( 0x00 );
    resolveIncludes( buf );

    auto *shader = NS::String::string( (const char*)buf.data(), NS::UTF8StringEncoding);
    MTL::CompileOptions *compileOptions = MTL::CompileOptions::alloc()->init();

    // NS::String* key1 = NS::String::string("ROOT_PATH", NS::UTF8StringEncoding);
    // NS::String* value1 = NS::String::string("/user/home/", NS::UTF8StringEncoding);
    // NS::Dictionary* macros = NS::Dictionary::dictionary( value1, key1 );
    // compileOptions->setPreprocessorMacros( macros );

    auto *errorDictionary = NS::Dictionary::dictionary();
    auto *errorMessages = NS::Error::alloc()->init(NS::CocoaErrorDomain, 99, errorDictionary);
    MTL::Library* library = device->newLibrary(shader, compileOptions, &errorMessages); 
    /*
      // https://developer.apple.com/documentation/metal/shader_libraries/generating_and_loading_a_metal_library_symbol_file?language=objc
      TBuffer buf2;
      buf2.load( "data/shaders.metallib");
      //NS::String* filepath = NS::String::string( "data/shaders.metallib", NS::UTF8StringEncoding);
      dispatch_data_t ddt = dispatch_data_create( buf2.data(), buf2.size(), nullptr, nullptr );
      library = device->newLibrary(ddt, &errorMessages); 
    */
    if( !library ) {
        fatal( "Failed to create shader library from %s\n%s", full_name.c_str(), errorMessages->localizedDescription()->utf8String() );
        return false;
    }
    setDbgName( library, full_name.c_str() );

    MTL::Function* vertFunc = library->newFunction( NS::String::string(vsFn.c_str(), NS::UTF8StringEncoding));
    if( !vertFunc ) 
      fatal( "Failed to compile VS %s @ %s\n", vsFn.c_str(), full_name.c_str() );
    MTL::Function* fragFunc = nullptr;
    if( !psFn.empty() ) {
      fragFunc = library->newFunction( NS::String::string(psFn.c_str(), NS::UTF8StringEncoding));
      if( !fragFunc ) 
        fatal( "Failed to compile PS %s @ %s\n", psFn.c_str(), full_name.c_str() );
    }

    //setDbgName( vertFunc, TStr256( "%s @ %s", vsFn.c_str(), full_name ).c_str() );
    // if( fragFunc )
    //   setDbgName( fragFunc, TStr256( "%s @ %s", psFn.c_str(), full_name ).c_str() );

    MTL::RenderPipelineDescriptor *desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vertFunc);
    desc->setFragmentFunction(fragFunc);
    // PixelFormatBGRA8Unorm_sRGB
    MTL::RenderPipelineColorAttachmentDescriptor* color_desc = desc->colorAttachments()->object(0);
    assert( color_desc );
    if( fragFunc ) 
      color_desc->setPixelFormat( MTL::PixelFormatBGRA8Unorm );
    else
      color_desc->setPixelFormat( MTL::PixelFormatInvalid );
    desc->setDepthAttachmentPixelFormat( MTL::PixelFormat::PixelFormatDepth16Unorm );
    
    switch( blend_cfg ) {
    case eBlendConfig::COMBINATIVE:
      color_desc->setBlendingEnabled( true );
      color_desc->setDestinationRGBBlendFactor( MTL::BlendFactor::BlendFactorOneMinusSourceAlpha );
      color_desc->setSourceRGBBlendFactor( MTL::BlendFactor::BlendFactorSourceAlpha );
      break;
    case eBlendConfig::DEFAULT:
      break;
    default:
      fatal( "Blend configuration %d not implemented in Metal", (int)blend_cfg );
    }

    pipeline_state = device->newRenderPipelineState(desc, &errorMessages);
    if( !pipeline_state ) {
      fatal( "%s", errorMessages->localizedDescription()->utf8String() );
      return false;
    }

    SAFE_RELEASE(vertFunc);
    SAFE_RELEASE(fragFunc);
    SAFE_RELEASE(desc);

    return true;
  }
  
  void PipelineState::setName( const char* new_name ) {
    assert( pipeline_state ); 
    IResource::setName( new_name );
    //desc->setLabel( NS::String::string( new_name, NS::UTF8StringEncoding ) );
  }

  void PipelineState::destroy() {
  	SAFE_RELEASE( pipeline_state );
  }

  // ---------------------------------------------------------------
  bool RenderToTexture::createRT(
    const char* rt_name,
    int new_xres, int new_yres,
    eFormat color_fmt, eFormat depth_fmt
  ) {

    width = new_xres;
    height = new_yres;

    assert( color_fmt == Render::eFormat::INVALID );

    // Expose depth_srv as a independent texture
    if (!depth_texture)
      depth_texture = new Texture;
    else
      depth_texture->destroy();

    MTL::PixelFormat mtl_depth_fmt = asMetalFormat( depth_fmt );
    if( mtl_depth_fmt == MTL::PixelFormatR16Unorm )
      mtl_depth_fmt = MTL::PixelFormatDepth16Unorm;

    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType( MTL::TextureType2D );
    desc->setWidth( width );
    desc->setHeight( height );
    desc->setDepth( 1 );
    desc->setPixelFormat( mtl_depth_fmt );
    desc->setArrayLength( 1 );
    desc->setMipmapLevelCount( 1 );
    desc->setStorageMode( MTL::StorageModePrivate );
    desc->setUsage( MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);
    depth_texture->texture = device->newTexture( desc );
    assert( depth_texture->texture );

    std::string z_new_name( rt_name );
    z_new_name += "_z.texture";

    dbg( "Creating depth rt texture called %s\n", z_new_name.c_str() );
    //setDbgName( texture, z_new_name );
    depth_texture->setName( z_new_name.c_str() );
    addResource(depth_texture);
    mtl_depth_texture = depth_texture->texture;
    return true;
  }

  void RenderToTexture::destroy() {
    RenderToTexture::destroyRT();
    Texture::destroy();
  }

  void RenderToTexture::beginRendering() {
    MTL::RenderPassDescriptor* render_pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();

    MTL::RenderPassDepthAttachmentDescriptor* depth = render_pass_descriptor->depthAttachment();
    assert( mtl_depth_texture );
    depth->setTexture( mtl_depth_texture );
    depth->setLoadAction( MTL::LoadActionClear );
    depth->setStoreAction( MTL::StoreActionStore );
    depth->setClearDepth( 1.0 );

    backbuffer_commandBuffer = RenderPlatform::getCommandBuffer();
    assert( backbuffer_commandBuffer );
    static Render::Encoder encoder;
    encoder.encoder = backbuffer_commandBuffer->renderCommandEncoder( render_pass_descriptor );
    assert( encoder.encoder );
    Render::setMainEncoder( &encoder );
  }

  void RenderToTexture::endRendering() {
    Render::Encoder* encoder = Render::getMainEncoder();
    encoder->encoder->endEncoding();
    assert( backbuffer_commandBuffer );
    backbuffer_commandBuffer->commit();
    Render::setMainEncoder( nullptr );
    backbuffer_commandBuffer = nullptr;
  }

}
