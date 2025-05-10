struct TVtxPos {
  VEC3 position;
};

struct TVtxPosColor {
  VEC3 position;
  VEC4 color;
#ifndef PLATFORM_HLSL
  TVtxPosColor( VEC3 in_pos, VEC4 in_color ) : position( in_pos ), color( in_color ) { }
  TVtxPosColor() = default;
#endif
};

struct TVtxPosN {
	VEC3 position;
	VEC3 normal;
#ifndef PLATFORM_HLSL
	TVtxPosN(VEC3 in_pos, VEC3 in_normal) : position(in_pos), normal(in_normal) { }
	TVtxPosN() = default;
#endif
};

struct TVtxPosNUv {
	VEC3 position;
	VEC3 normal;
	VEC2 uv;
#ifndef PLATFORM_HLSL
	TVtxPosNUv(VEC3 in_pos, VEC3 in_normal, VEC2 in_uv) : position(in_pos), normal(in_normal), uv(in_uv) { }
	TVtxPosNUv() = default;
#endif
};
