#include "common.hlsl"

struct v2f {
  float4 position : SV_POSITION;
  half4  color    : COLOR;
  float2 uv : TEXCOORD0;
};

struct TSprite {
  float3 pos;
  float  scale;
  float4 color;
};

// ----------------------------------------------
v2f VS(
    in uint vID : SV_VertexID,
    in uint instanceId : SV_InstanceID,
    StructuredBuffer<TVtxPosColor> vtxs : register(t0),
    StructuredBuffer<CteCamera> cameras : register(t1),
    StructuredBuffer<TSprite> instances : register(t2)
  )
  {
    const TVtxPosColor vtx = vtxs[vID];
    const TSprite instance = instances[ instanceId];
    const CteCamera camera = cameras[0];

    float3 wPos = instance.pos + (-vtx.position.x * camera.left.xyz + vtx.position.y * camera.up.xyz) * instance.scale * 0.1;
    v2f v_out;
    v_out.position = mul(float4(wPos, 1.0), camera.view_proj);
    v_out.uv = vtx.position.xy * 2.0; // -float2(0.5, 0.5);
    v_out.color = instance.color;
    return v_out;
  }

// ----------------------------------------------
float4 PS(v2f v) : SV_Target{
  float a = 1.0 - saturate(length(v.uv));
 if (a < 0.2)
    discard;
  return float4(a,a,a,1) * v.color;
}
