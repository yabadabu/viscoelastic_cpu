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
    StructuredBuffer<TVtxPos> vtxs : register(t0),
    StructuredBuffer<CteCamera> cameras : register(t1),
    StructuredBuffer<TSprite> instances : register(t2)
  )
  {
    const CteCamera camera = cameras[0];
    TSprite instance = instances[instanceId];
    TVtxPos vtx = vtxs[vID];
    float3 wPos = instance.pos + (-vtx.position.x * camera.left.xyz + vtx.position.y * camera.up.xyz) * instance.scale * 0.1;
    v2f v_out;
    v_out.position = mul(float4(wPos, 1.0), camera.view_proj);
    v_out.uv = vtx.position.xy - float2(0.5, 0.5);
    v_out.color = instance.color;
    return v_out;
  }

// ----------------------------------------------
float4 PS(v2f v) : SV_Target{
  return float4(1.0, 0.0, 0.0, 1.0);
  float a = saturate(1.0 - length(v.uv - float2(0.5,0.5)) * 2);
  if (a < 0.2)
    discard;
  float lum = length(v.uv);
  return float4(lum,lum,lum,1) * v.color;
}
