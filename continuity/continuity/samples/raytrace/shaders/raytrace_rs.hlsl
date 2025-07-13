#ifndef RAYTRACE_HLSL
#define RAYTRACE_HLSL

#include "shaders/common.hlsli"
#include "shared/raytracecommon.h"
#include "shaders/lighting.hlsli"

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
    ConstantBuffer<rt::sceneconstants> frameconstants = ResourceDescriptorHeap[2];
    
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

    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[3];
    TraceRay(scene, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, rayDesc, raypayload);

    RWTexture2D<float4> rendertarget = ResourceDescriptorHeap[4];

    // write the raytraced color to the output texture.
    rendertarget[DispatchRaysIndex().xy] = raypayload.color;
}

[shader("closesthit")]
void closesthitshader_triangle(inout raypayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    StructuredBuffer<float3> vertexbuffer = ResourceDescriptorHeap[5];
    StructuredBuffer<uint> indexbuffer = ResourceDescriptorHeap[6];
    ConstantBuffer<rt::sceneconstants> frameconstants = ResourceDescriptorHeap[2];
    StructuredBuffer<uint> material_ids = ResourceDescriptorHeap[7];
    StructuredBuffer<material> materials = ResourceDescriptorHeap[8];

    // index of blas in tlas, assume each instance only contains geometries that use same material id
    uint const geometryinstance_idx = InstanceIndex();

    material geomaterial = materials[material_ids[geometryinstance_idx]];

    uint const triidx = PrimitiveIndex();
    
    float3 const v0 = vertexbuffer[indexbuffer[triidx * 3]];
    float3 const v1 = vertexbuffer[indexbuffer[triidx * 3 + 1]];
    float3 const v2 = vertexbuffer[indexbuffer[triidx * 3 + 2]];

    float3 const barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);    
    float3 const hitpoint = barycentrics.x * v0 + barycentrics.y * v1 + barycentrics.z * v2;

    // counter-clockwise winding
    float3 n = normalize(cross(v1 - v0, v2 - v0));
    float3 l = -frameconstants.sundir;
    float3 v = normalize(frameconstants.campos - hitpoint);
   
    payload.color.a = 1.0f;
    payload.color.xyz = calculatelighting(float3(50, 50, 50), l, v, n, geomaterial.colour.xyz, geomaterial.roughness, geomaterial.reflectance, geomaterial.metallic) + float3(0.3, 0.3, 0.3); // ambient term
}

#endif // RAYTRACE_HLSL