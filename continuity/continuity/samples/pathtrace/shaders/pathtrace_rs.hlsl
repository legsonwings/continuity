#ifndef RAYTRACE_HLSL
#define RAYTRACE_HLSL

#include "shaders/common.hlsli"
#include "shared/raytracecommon.h"
#include "shaders/lighting.hlsli"

ConstantBuffer<rt::rootdescs> rootdescriptors : register(b0);

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
    StructuredBuffer<rt::dispatchparams> descriptorsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc]; 
    rt::dispatchparams descriptors = descriptorsbuffer[0];

    StructuredBuffer<rt::viewglobals> view = ResourceDescriptorHeap[descriptors.viewglobals];

    // generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    ray ray = generateray(DispatchRaysIndex().xy, view[0].viewpos, view[0].invviewproj);

    // set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;

    // set TMin to a zero value to avoid aliasing artifacts along contact areas.
    // Note: make sure to enable face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    raypayload raypayload = { float4(0, 0, 0, 0)};

    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[descriptors.accelerationstruct];
    TraceRay(scene, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, rayDesc, raypayload);

    RWTexture2D<float4> rendertarget = ResourceDescriptorHeap[descriptors.rtoutput];

    // write the raytraced color to the output texture.
    rendertarget[DispatchRaysIndex().xy] = raypayload.color;
}

[shader("closesthit")]
void closesthitshader_triangle(inout raypayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    StructuredBuffer<rt::dispatchparams> descriptorsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams descriptors = descriptorsbuffer[0];

    StructuredBuffer<rt::viewglobals> view = ResourceDescriptorHeap[descriptors.viewglobals];
    StructuredBuffer<rt::sceneglobals> scene = ResourceDescriptorHeap[descriptors.sceneglobals];

    StructuredBuffer<float3> vertexbuffer = ResourceDescriptorHeap[descriptors.posbuffer];
    StructuredBuffer<index> indexbuffer = ResourceDescriptorHeap[descriptors.indexbuffer];
    StructuredBuffer<uint> primmaterialsbuffer = ResourceDescriptorHeap[descriptors.primitivematerialsbuffer];
    StructuredBuffer<material> materials = ResourceDescriptorHeap[scene[0].matbuffer];

    // index of blas in tlas, assume each instance only contains geometries that use same material id
    //uint const geometryinstance_idx = InstanceIndex();

    uint const triidx = PrimitiveIndex();
    material mat = materials[primmaterialsbuffer[triidx]];
    
    float3 const v0 = vertexbuffer[indexbuffer[triidx * 3].pos];
    float3 const v1 = vertexbuffer[indexbuffer[triidx * 3 + 1].pos];
    float3 const v2 = vertexbuffer[indexbuffer[triidx * 3 + 2].pos];

    float3 const barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);    
    float3 const hitpoint = barycentrics.x * v0 + barycentrics.y * v1 + barycentrics.z * v2;

    // counter-clockwise winding
    // todo : use vertex normals passed in
    // todo : shade using textures
    float3 n = normalize(cross(v1 - v0, v2 - v0));
    float3 l = -scene[0].lightdir;
    float3 v = normalize(view[0].viewpos - hitpoint);
   
    payload.color.a = 1.0f;
    payload.color.xyz = calculatelighting(float3(50, 50, 50), l, v, n, mat.colour.xyz, mat.roughness, mat.reflectance, mat.metallic) + float3(0.3, 0.3, 0.3); // ambient term
}

#endif // RAYTRACE_HLSL