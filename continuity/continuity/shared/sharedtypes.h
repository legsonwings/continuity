#pragma once

#if __cplusplus

import std;
import vec;

#define float4 stdx::vec4
#define float3 stdx::vec3
#define float2 stdx::vec2

#define float3x3 stdx::matrix3x3
#define float4x4 stdx::matrix4x4

#else

#define uint32 uint

#endif