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
    CteObject    object = objects[ off2 + instanceId ];

    v2f vout;
    float4 world_pos = TRANSFORM_POINT( in_vtx.position, object.world);
    vout.position = TRANSFORM_VEC4( world_pos, cameras[0].view_proj );
    vout.color = half4(in_vtx.color * object.color);
    return vout;
}

// ----------------------------------------------
v2f VS_Solid(
    in uint vID : SV_VertexID,
    in uint instanceId : SV_InstanceID,
    StructuredBuffer<TVtxPosN> vertices : register(t0),
    StructuredBuffer<CteCamera> cameras : register(t1),
    StructuredBuffer<CteObject> objects : register(t2)
){
    const TVtxPosN  in_vtx = vertices[ vID ];
    const CteObject object = objects[ off2 + instanceId ];
    const CteCamera camera = cameras[ 0 ];

    v2f vout;
    float4 world_pos = TRANSFORM_POINT( in_vtx.position, object.world );
    vout.position = TRANSFORM_VEC4(world_pos, camera.view_proj);
    float3 world_n = normalize(TRANSFORM_DIR(in_vtx.normal, object.world));

    float diffuse_amount = dot( -camera.forward, world_n );
    //diffuse_amount = clamp( diffuse_amount );

    vout.color.xyz = half3(object.color.xyz * diffuse_amount);
    vout.color.w = 1.0;
    return vout;
}

// ----------------------------------------------
float4 PS(v2f iv) : SV_Target
{
    return iv.color;
}
