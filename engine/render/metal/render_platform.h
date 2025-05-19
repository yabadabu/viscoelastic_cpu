#pragma once

#include "Metal/Metal.hpp"
#include "QuartzCore/CAMetalDrawable.hpp"
#include <string>

namespace RenderPlatform {

	enum ePrimitiveType {
		eLines =     MTL::PrimitiveTypeLine,
		eTriangles = MTL::PrimitiveTypeTriangle,
	};

	struct Mesh {
		MTL::Buffer*   ib = nullptr;
		MTL::Buffer*   vb = nullptr;
	};

	enum eCullMode {
		eBack  = MTL::CullModeBack,
		eFront = MTL::CullModeFront,
		eNone  = MTL::CullModeNone,
		eDefault  = MTL::CullModeBack,
	};

	struct VertexDecl {
	};

	struct Texture {
		MTL::Texture*  texture = nullptr;
	};

	// -----------------------------------------
	struct RenderToTexture {
		int                       width = 0;
		int                       height = 0;
		MTL::Texture*             mtl_depth_texture = nullptr;
		void beginRendering();
		void endRendering();
		void destroyRT();
		void clearRT(VEC4 color);
		void clearDepth(float clearDepthValue = 1.0f);
	};	

	struct PipelineState {
		std::string src;
		std::string vsFn;
		std::string psFn;
		MTL::RenderPipelineState* pipeline_state = nullptr;
	};

	struct Buffer {
		MTL::Buffer*   buffer = nullptr;
		uint32_t       current_offset = 0;
		uint32_t       current_frame = 0;
	};

	struct Encoder {
		MTL::RenderCommandEncoder* encoder = nullptr;
	};

	bool create();
	void destroy();
  MTL::Device* getDevice();
  MTL::CommandBuffer* getCommandBuffer();
  void beginFrame( uint32_t frame_id, MTL::RenderPassDescriptor* passDescriptor
                 , MTL::CommandBuffer* new_commandBuffer, CA::MetalDrawable* drawable );
	void beginRenderingBackBuffer();
	void endRenderingBackBuffer();


}

