#include "platform.h"
#include "render.h"

using namespace Render;

static void addLine( std::vector< TVtxPosColor >& vtxs, VEC3 a, VEC3 b, VEC4 clr ) {
  vtxs.emplace_back(a, clr);
  vtxs.emplace_back(b, clr);
}

static void fromLines( Render::Mesh* mesh, std::vector< TVtxPosColor >& vtxs ) {
  mesh->create(vtxs.data(), (uint32_t)vtxs.size(), Render::eLines);
}

// --------------------------------------------------------------------------------
void createAxis(Render::Mesh* mesh) {
  std::vector< TVtxPosColor > vtxs;
  addLine( vtxs, VEC3::zero, VEC3::axis_x, Color::Red );
  addLine( vtxs, VEC3::zero, 2 * VEC3::axis_y, Color::Green );
  addLine( vtxs, VEC3::zero, 3 * VEC3::axis_z, Color::Blue );
  fromLines( mesh, vtxs );
}

// --------------------------------------------------------------------------------
void createGrid(Render::Mesh* mesh, float sz, int nsamples, int nsubsamples) {
  int ntotal = nsamples * nsubsamples;

  std::vector< TVtxPosColor > vtxs;
  float c0 = 0.1f;
  float c1 = 0.3f;
  VEC4 color0 = VEC4(c0, c0, c0, 1.0f);
  VEC4 color1 = VEC4(c1, c1, c1, 1.0f);
  float fn = (float)ntotal * sz;
  for( int k = 0; k<2; ++k ) {
    for (int i = -ntotal; i <= ntotal; ++i) {
      // First the dark lines, then the lighter
      if( k != (( i % 5 ) == 0 ))
        continue;
      VEC4 clr = (i % 5) ? color0 : color1;
      float fi = (float)i;
      
      addLine( vtxs, VEC3(fn, 0, fi), VEC3(-fn, 0, fi), clr);
      addLine( vtxs, VEC3(fi, 0, fn), VEC3(fi, 0, -fn), clr);
    }
  }
  fromLines( mesh, vtxs );
}

// --------------------------------------------------------------------------------
void createUnitWiredCube(Render::Mesh* mesh) {
  const float sz = 0.5f;
  const VEC3 coords[8] = {
    VEC3( -sz, -sz, -sz),
    VEC3( sz, -sz, -sz),
    VEC3( -sz, -sz, sz),
    VEC3( sz, -sz, sz),
    VEC3( -sz, sz, -sz),
    VEC3( sz, sz, -sz),
    VEC3( -sz, sz, sz),
    VEC3( sz, sz, sz),
  };
  const uint32_t nindices = 24;
  uint16_t indices[nindices];
  uint16_t* ip = indices;
  for (uint16_t i = 0; i < 4; ++i) {
    *ip++ = i;
    *ip++ = i + 4;
    *ip++ = i * 2;
    *ip++ = i * 2 + 1;
    if (i < 2) {
      *ip++ = i;
      *ip++ = i + 2;
    }
    else {
      *ip++ = 2 + i;
      *ip++ = 2 + i + 2;
    }
  }
  
  // De-index the lines
  std::vector< TVtxPosColor > vtxs;
  for( int i=0; i<nindices; i+=2 ) {
    VEC3 a = coords[ indices[i] ];
    VEC3 b = coords[ indices[i+1] ];
    addLine( vtxs, a, b, Color::White );
  }
  fromLines( mesh, vtxs );
}

// -----------------------------------------------------
void createUnitWiredCircleXZ(Render::Mesh* mesh, int nsamples) {
  assert(nsamples > 2);
  std::vector< TVtxPosColor > vtxs;
  float du = 2.0f * (float)(M_PI) / (float)(nsamples);
  VEC3 p = getVectorFromYaw(0.0f);
  for (int i = 1; i <= nsamples; ++i) {
    VEC3 p1 = getVectorFromYaw(i * du);
    addLine( vtxs, p, p1, Color::White );
    p = p1;
  }
  fromLines( mesh, vtxs );
}

// -----------------------------------------------------
void createViewVolume(Render::Mesh* mesh) {
  const float sz = 1.f;
  const VEC3 coords[8] = {
    VEC3(-sz, -sz, 0.f),
    VEC3( sz, -sz, 0.f),
    VEC3(-sz,  sz, 0.f),
    VEC3( sz,  sz, 0.f),
    VEC3(-sz, -sz, 1.f),
    VEC3( sz, -sz, 1.f),
    VEC3(-sz,  sz, 1.f),
    VEC3( sz,  sz, 1.f),
  };
  const uint32_t nindices = 24;
  uint16_t indices[nindices] = {
    0, 1, 2, 3, 4, 5, 6, 7
  , 0, 2, 1, 3, 4, 6, 5, 7
  , 0, 4, 1, 5, 2, 6, 3, 7
  };
  
  std::vector< TVtxPosColor > vtxs;
  for( int i=0; i<nindices; i+=2 ) {
    VEC3 a = coords[ indices[i] ];
    VEC3 b = coords[ indices[i+1] ];
    addLine( vtxs, a, b, Color::White );
  }
  
  fromLines( mesh, vtxs );
}

// -----------------------------------------------------
void createLine(Render::Mesh* mesh) {
  std::vector< TVtxPosColor > vtxs;
  addLine( vtxs, VEC3::zero, VEC3::axis_z, Color::White );
  fromLines( mesh, vtxs );
}

// -----------------------------------------------------
// Without Normals
void createQuadXYBasic(Render::Mesh* mesh) {
	std::vector< TVtxPosColor > vtxs;
	VEC4 color = Color::White;
	float sz = 1.0f;
	VEC3 p0(0, 0, 0);
	VEC3 v1(sz, 0, 0);
	VEC3 v2(0, sz, 0);
	vtxs.emplace_back(p0, color);
	vtxs.emplace_back(p0 + v1, color);
	vtxs.emplace_back(p0 + v2, color);
	vtxs.emplace_back(p0 + v1 + v2, color);
	uint16_t indices[6] = { 0,2,1, 1,2,3 };
	mesh->create(vtxs.data(), (uint32_t)vtxs.size(), Render::eTriangles, indices, 6);
}

void createQuadXZ(Render::Mesh* mesh, float sz) {
  std::vector< TVtxPosN > vtxs;
  float szh = sz * 0.5f;
  VEC3 p0( -szh, 0.0f, -szh);
  VEC3 v1( 0, 0, sz );
  VEC3 v2( sz, 0, 0 );
  VEC3 n = v1.cross( v2 ).normalized();
  vtxs.emplace_back( p0, n );
  vtxs.emplace_back( p0 + v1, n );
  vtxs.emplace_back( p0 + v2, n );
  vtxs.emplace_back( p0 + v1 + v2, n );
  uint16_t indices[6] = { 0,2,1, 1,2,3 };
  mesh->create(vtxs.data(), (uint32_t)vtxs.size(), Render::eTriangles, indices, 6 );
}

void createQuadXY(Render::Mesh* mesh, float sz, VEC2 center_at) {
	std::vector< TVtxPosN > vtxs;
	float szh = sz * 0.5f;
	VEC3 p0 = VEC3(center_at.x - szh, center_at.y - szh, 0);
	VEC3 v1(0, sz, 0);
	VEC3 v2(sz, 0, 0);
	VEC3 n = v2.cross(v1).normalized();
	vtxs.emplace_back(p0, n);
	vtxs.emplace_back(p0 + v1, n);
	vtxs.emplace_back(p0 + v2, n);
	vtxs.emplace_back(p0 + v1 + v2, n);
	uint16_t indices[6] = { 0,1,2, 2,1,3 };
	mesh->create(vtxs.data(), (uint32_t)vtxs.size(), Render::eTriangles, indices, 6);
}

void createUnitQuadXY(Render::Mesh* mesh) {
  std::vector< TVtxPosColor > vtxs;
  VEC4 color = Color::White;
  const float sz = 0.5f;
  vtxs.emplace_back(VEC3(-sz, -sz, 0), color);
  vtxs.emplace_back(VEC3(-sz, sz, 0), color);
  vtxs.emplace_back(VEC3(sz, sz, 0), color);
  vtxs.emplace_back(VEC3(sz, sz, 0), color);
  vtxs.emplace_back(VEC3(sz, -sz, 0), color);
  vtxs.emplace_back(VEC3(-sz, -sz, 0), color);
  mesh->create(vtxs.data(), (uint32_t)vtxs.size(), Render::eTriangles);
}

// subdivs = 0 => 4 vertices
void createQuadXYTextured(Render::Mesh* mesh, float sz, int subdivs, VEC2 center_at) {
	std::vector< TVtxPosNUv > vtxs;
	float szh = sz * 0.5f;
	VEC3 p0 = VEC3(center_at.x - szh, center_at.y - szh, 0);
	VEC3 v1(0, sz, 0);
	VEC3 v2(sz, 0, 0);
	VEC3 n = v2.cross(v1).normalized();
	int nfaces_per_axis = (subdivs + 1);
	int nvertices_per_axis = (subdivs + 2);
    int nvertices_total = nvertices_per_axis * nvertices_per_axis;
    vtxs.reserve( nvertices_total );

    int nquads = ( nvertices_per_axis - 1 ) * (nvertices_per_axis - 1);
    std::vector< uint16_t > indices;
    indices.reserve(nquads * 6);
	for (int iy = 0; iy <= nfaces_per_axis; ++iy) {
        float uy = iy / (float)nfaces_per_axis;
		for (int ix = 0; ix <= nfaces_per_axis; ++ix) {
			float ux = ix / (float)nfaces_per_axis;
            VEC3 p = p0 + v2 * ux + v1 * uy;
			vtxs.emplace_back(p, n, VEC2(ux, 1.0f - uy));
		}
	}

	for (int iy = 0; iy < nfaces_per_axis; ++iy) {
		for (int ix = 0; ix < nfaces_per_axis; ++ix) {
            int i0 = iy * nvertices_per_axis + ix;
			indices.push_back(i0);
			indices.push_back(i0 + nvertices_per_axis);
			indices.push_back(i0 + 1);

			indices.push_back(i0 + nvertices_per_axis);
			indices.push_back(i0 + nvertices_per_axis + 1);
			indices.push_back(i0 + 1);
        }
    }
	mesh->create(vtxs.data(), (uint32_t)vtxs.size(), Render::eTriangles, indices.data(), (uint32_t)indices.size());
}


// -----------------------------------------------------
void addFace( std::vector< TVtxPosN >& v, VEC3 p0, VEC3 v1, VEC3 v2 ) {
  VEC3 n = v1.cross( v2 ).normalized();
  VEC3 p1 = p0 + v1;
  VEC3 p2 = p0 + v2;
  VEC3 p3 = p0 + v1 + v2;
  v.emplace_back( p0, n);
  v.emplace_back( p3, n);
  v.emplace_back( p1, n);
  v.emplace_back( p3, n);
  v.emplace_back( p0, n);
  v.emplace_back( p2, n);
}

// This is NOT centered in the origin, but on top of the plane XZ
void createUnitCube(Render::Mesh* mesh, float sz) {
  std::vector< TVtxPosN > vtxs;
  const float szh = sz * 0.5f;
  addFace( vtxs, VEC3( -szh, sz, -szh), VEC3( 0, 0, sz ), VEC3( sz, 0, 0 ));      // up
  addFace( vtxs, VEC3( -szh, 0.0f, -szh), VEC3( sz, 0, 0), VEC3( 0, 0, sz ));     // dw
  addFace( vtxs, VEC3(  szh, sz, -szh), VEC3( 0, 0, sz ), VEC3( 0, -sz, 0 ));
  addFace( vtxs, VEC3( -szh, 0.0f, -szh), VEC3( 0, 0, sz ), VEC3( 0, sz, 0 ));
  addFace( vtxs, VEC3( -szh, sz, szh), VEC3( 0, -sz, 0 ), VEC3( sz, 0, 0 ));
  addFace( vtxs, VEC3( -szh, 0.0f, -szh), VEC3( 0, sz, 0 ), VEC3( sz, 0, 0 ));
  mesh->create(vtxs.data(), (uint32_t)vtxs.size(), Render::eTriangles );
}
