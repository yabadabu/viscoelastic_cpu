// HLSL uses this definition
#define ShaderBuffer(name,cte) cbuffer name : register(b##cte)
#define USE_SHADER_REG(nreg) : register(t ## nreg)
#define MAT44            matrix
#define VEC4             float4
#define VEC3             float3
#define VEC2             float2
#define u32              uint
#define TRANSFORM_POINT(point,mtx) mul(float4(point,1.0), mtx)
#define TRANSFORM_VEC4(point,mtx) mul(point, mtx)
#define TRANSFORM_DIR(point,mtx) mul(float4(point,0.0), mtx).xyz
#define TRANSLATION_FROM_MATRIX(mtx) float3(mtx._m30, mtx._m31, mtx._m32)
#define AXIS_Z_FROM_MATRIX(mtx) float3(mtx._m20, mtx._m21, mtx._m22 )

#include "data/shaders/vertex_types.h"
#include "data/shaders/constants.h"

#define ShaderBuffer(name,cte) cbuffer name : register(b##cte)
cbuffer BuffersOffsets : register(b0) {
  uint  off0;
  uint  off1;
  uint  off2;
  uint  off3;
  uint  off4;
  uint  off5;
  uint  off6;
  uint  off7;
};
