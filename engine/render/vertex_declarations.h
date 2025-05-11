#include "vertex_types.h"

namespace Render {

	struct VertexDecl;

	const VertexDecl* getVertexDeclByName( const char* name );

	template< typename T >
	const VertexDecl* getVertexDecl( );

}