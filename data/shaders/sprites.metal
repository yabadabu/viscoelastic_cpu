#include "data/shaders/common.metal"

struct v2f {
  float4 position [[position]];
  half2  uv;
  half4  color;
};

struct TSprite {
  VEC3  pos;
  float scale;
  VEC4  color;
};

// ----------------------------------------------
vertex v2f VS(
    const device TVtxPosColor* vertices [[buffer(0)]],
    constant CteCamera& camera [[buffer(1)]],
    const device TSprite* sprites [[buffer(2)]],
    unsigned int vID[[vertex_id]],
    uint instanceId [[instance_id]]
){

    const device TVtxPosColor& vtx = vertices[ vID ];
    const device TSprite& object = sprites[ instanceId ];

    float3 wPos = object.pos + (-vtx.position.x * camera.left.xyz + vtx.position.y * camera.up.xyz) * object.scale * 0.1;
    v2f vout;
    float4 world_pos( wPos, 1.0 );
    vout.position = TRANSFORM_VEC4( world_pos, camera.view_proj );
    vout.uv = half2(vtx.position.xy * 2.0);
    vout.color = half4(object.color);
    return vout;
}
// ----------------------------------------------
fragment half4 PS(v2f v [[stage_in]])
{
  float a = 1.0 - saturate(length(v.uv));
  if (a < 0.2)
     discard_fragment();
  return half4(a,a,a,1) * v.color;
}

