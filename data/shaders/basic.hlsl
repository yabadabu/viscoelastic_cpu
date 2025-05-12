#include "common.hlsl"

struct v2f {
  float4 position : SV_POSITION;
  half4  color    : COLOR;
};

// ----------------------------------------------
v2f VS(
    in uint vID : SV_VertexID,
    in uint instanceId : SV_InstanceID,
    StructuredBuffer<TVtxPosColor> vertices : register(t0),
    StructuredBuffer<CteCamera> cameras : register(t1),
    StructuredBuffer<CteObject> objects : register(t2)
) {

    TVtxPosColor in_vtx = vertices[vID];
    CteObject    object = objects[off2 + instanceId];

    v2f vout;
    float4 world_pos = TRANSFORM_POINT( in_vtx.position, object.world);
    vout.position = TRANSFORM_VEC4( world_pos, cameras[0].view_proj );
    vout.color = half4(in_vtx.color * object.color);
    return vout;
}

// ----------------------------------------------
float4 PS(v2f iv) : SV_Target
{
    return iv.color;
}
