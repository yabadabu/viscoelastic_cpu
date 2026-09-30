#include "platform.h"
#include "render/render.h"

VEC2 mouse_cursor;

namespace RenderPlatform {
	void beginRenderingBackBuffer() {
	}
	void endRenderingBackBuffer() {
	}
}

namespace Render {
	bool Mesh::create( 
		const void* in_data, 
		uint32_t in_nvertices, 
		ePrimitiveType in_primitive_type, 
		const void* in_index_data,
		uint32_t in_nindices,
		uint32_t in_bytes_per_index,
		const VertexDecl* in_vertex_decl 
	) {
		return true;
	}
	void Buffer::setName( const char* new_name ) { 
		IResource::setName(new_name);
	}
	bool Buffer::create( size_t total_bytes ) { return true; }
	void Buffer::destroy() { }
	void* Buffer::rawData() { return nullptr; }

	void Encoder::drawInstancedMesh(Render::Mesh const*, int, unsigned int) { }
	void Encoder::setVertexBufferOffset( int instance_idx, int bytes_per_instance, int slot ) {	}
	void Encoder::setRenderPipelineState( const PipelineState* in_pipeline ) { }
	void Encoder::setBufferContents( Buffer* buffer, const void* new_data, size_t new_data_size ) { }
	void Encoder::setVertexBuffer(Render::Buffer const*) { }

 	bool PipelineState::reloadShaders() { return true; }
	bool PipelineState::create( const json& j ) { return true; }
	void PipelineState::setName( const char* new_name ) { 
		IResource::setName(new_name);
	}
	void PipelineState::destroy() { }

	void Mesh::destroy() { }
	void Mesh::setName( const char* new_name ) { 
		IResource::setName(new_name);
	}

}