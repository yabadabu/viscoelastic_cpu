#include "data/shaders/common.metal"

struct v2f {
  float4 position [[position]];
  half4  color;
};

// ----------------------------------------------
vertex v2f VS(
    const device TVtxPosColor* vertices [[buffer(0)]],
    constant CteCamera& camera [[buffer(1)]],
    const device CteObject* objects [[buffer(2)]],
    unsigned int vID[[vertex_id]],
    uint instanceId [[instance_id]]
){

    const device TVtxPosColor& in_vtx = vertices[ vID ];
    const device CteObject& object = objects[ instanceId ];

    v2f vout;
    float4 world_pos = TRANSFORM_POINT( in_vtx.position, object.world );
    vout.position = TRANSFORM_VEC4( world_pos, camera.view_proj );
    vout.color = half4(in_vtx.color * object.color);
    return vout;
}

// ----------------------------------------------
vertex v2f VS_Solid(
    const device TVtxPosN* vertices [[buffer(0)]],
    constant CteCamera& camera [[buffer(1)]],
    const device CteObject* objects [[buffer(2)]],
    unsigned int vID[[vertex_id]],
    uint instanceId [[instance_id]]
){
    TVtxPosN in_vtx = vertices[ vID ];
    CteObject object = objects[ instanceId ];

    v2f vout;
    float4 world_pos = TRANSFORM_POINT( in_vtx.position, object.world );
    vout.position = TRANSFORM_VEC4( world_pos, camera.view_proj );
    float3 world_n = normalize(TRANSFORM_DIR(in_vtx.normal, object.world));

    float diffuse_amount = dot( -camera.forward, world_n );
    //diffuse_amount = clamp( diffuse_amount );

    vout.color.xyz = half3(object.color.xyz * diffuse_amount);
    vout.color.w = 1.0;
    return vout;
}

// ----------------------------------------------
fragment half4 PS(v2f in [[stage_in]])
{
    return in.color;
}
