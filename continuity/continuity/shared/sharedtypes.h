#pragma once

#if __cplusplus

import std;
import vec;

#define uint2 stdx::vecui2
#define uint3 stdx::vecui3
#define uint4 stdx::vecui4

#define int2 stdx::veci2
#define int3 stdx::veci3
#define int4 stdx::veci4

#define float4 stdx::vec4
#define float3 stdx::vec3
#define float2 stdx::vec2

#define float3x3 stdx::matrix3x3
#define float4x4 stdx::matrix4x4

#else

#define uint32 uint

#endif