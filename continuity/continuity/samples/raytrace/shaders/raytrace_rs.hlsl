#ifndef RAYTRACE_HLSL
#define RAYTRACE_HLSL

#include "sharedconstants.h"

struct light
{
    float3 color;
    float range;
    float3 position;
    float padding1;
    float3 direction;
    float padding2;
};

struct sceneconstants
{
    float3 campos;
    uint padding0;
    float4 ambient;
    light lights[MAX_NUM_LIGHTS];
    float4x4 viewproj;
    uint numdirlights;
    uint numpointlights;
};

RaytracingAccelerationStructure g_scene : register(t0, space0);
RWTexture2D<float4> g_renderTarget : register(u0);
ConstantBuffer<sceneconstants> g_sceneCB : register(b0);

struct RayPayload
{
    float4 color;
};

struct Ray
{
    float3 origin;
    float3 direction;
};

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline Ray GenerateCameraRay(uint2 index, in float3 cameraPosition, in float4x4 projectionToWorld)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a world positon.
    float4 world = mul(float4(screenPos, 0, 1), projectionToWorld);
    world.xyz /= world.w;

    Ray ray;
    ray.origin = cameraPosition;
    ray.direction = normalize(world.xyz - ray.origin);

    return ray;
}

[shader("raygeneration")]
void MyRaygenShader()
{
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    Ray ray = GenerateCameraRay(DispatchRaysIndex().xy, g_sceneCB.campos, g_sceneCB.viewproj);

    // Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    // Set TMin to a zero value to avoid aliasing artifacts along contact areas.
    // Note: make sure to enable face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    RayPayload rayPayload = { float4(0, 0, 0, 0)};

    TraceRay(g_scene, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, rayDesc, rayPayload);

    // Write the raytraced color to the output texture.
    g_renderTarget[DispatchRaysIndex().xy] = rayPayload.color;
}

//***************************************************************************
//******************------ Closest hit shaders -------***********************
//***************************************************************************

[shader("closesthit")]
void MyClosestHitShader_Triangle(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    payload.color = float4(1.0, 0.0, 0.0, 1);
}

#endif // RAYTRACE_HLSL