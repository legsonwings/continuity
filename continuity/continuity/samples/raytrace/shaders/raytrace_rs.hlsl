#ifndef RAYTRACE_HLSL
#define RAYTRACE_HLSL

#include "samples/raytrace/raytracecommon.h"

ConstantBuffer<rt::rootconstants> rootconsts : register(b0);

struct raypayload
{
    float4 color;
};

struct ray
{
    float3 origin;
    float3 direction;
};

[shader("miss")]
void missshader(inout raypayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}

// generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline ray generateray(uint2 index, in float3 campos, in float4x4 projtoworld)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // unproject the pixel coordinate into a world positon.
    float4 world = mul(projtoworld, float4(screenPos, 0, 1));
    world.xyz /= world.w;

    ray ray;
    ray.origin = campos;
    ray.direction = normalize(world.xyz - ray.origin);

    return ray;
}

[shader("raygeneration")]
void raygenshader()
{
    ConstantBuffer<rt::sceneconstants> frameconstants = ResourceDescriptorHeap[rootconsts.frameidx];
    
    // generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    ray ray = generateray(DispatchRaysIndex().xy, frameconstants.campos, frameconstants.inv_viewproj);

    // set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;

    // set TMin to a zero value to avoid aliasing artifacts along contact areas.
    // Note: make sure to enable face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    raypayload raypayload = { float4(0, 0, 0, 0)};

    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[2];
    TraceRay(scene, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, rayDesc, raypayload);

    RWTexture2D<float4> rendertarget = ResourceDescriptorHeap[3];

    // write the raytraced color to the output texture.
    rendertarget[DispatchRaysIndex().xy] = raypayload.color;
}

[shader("closesthit")]
void closesthitshader_triangle(inout raypayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    payload.color = float4(1, 0, 0, 1);
}

#endif // RAYTRACE_HLSL