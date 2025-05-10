#include "vertex_types.h"

namespace Render {

	struct VertexDecl;

	ENGINE_API const VertexDecl* getVertexDeclByName( const char* name );

	template< typename T >
	const VertexDecl* getVertexDecl( );

}