#include <metal_stdlib>
using namespace metal;

#define VEC2             packed_float2
#define VEC3             packed_float3
#define VEC4             packed_float4
#define MAT44            float4x4
#define u32              uint
#define TRANSFORM_POINT(point,mtx) ((mtx)*(float4(point,1)))
#define TRANSFORM_DIR(point,mtx) ((mtx)*(float4(point,0))).xyz
#define TRANSFORM_VEC4(point,mtx) ((mtx)*(point))
#define TRANSLATION_FROM_MATRIX(mtx) float3(mtx[3][0], mtx[3][1], mtx[3][2] )
#define AXIS_Z_FROM_MATRIX(mtx) float3(mtx[2][0], mtx[2][1], mtx[2][2] )

#include "data/shaders/vertex_types.h"
#include "data/shaders/constants.h"

