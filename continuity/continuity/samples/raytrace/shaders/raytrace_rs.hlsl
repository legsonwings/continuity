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

// normal distribution function
// return the amount of microfacets that would reflect light along view direction
float ggx_specularndf(float noh, float r2)
{
    // see [Walter 07]
    float t = noh * noh * (r2 - 1.0f) + 1;

    return r2 / (rt::pi * t * t);
}

// visibility function that takes microfacet heights into account
float ggx_specularvisibility(float nol, float nov, float r2)
{
    // see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/normaldistributionfunction(speculard)
    float ggt1 = nol * sqrt(nov * nov * (1.0f - r2) + r2);
    float ggt2 = nov * sqrt(nol * nol * (1.0f - r2) + r2);

    return 0.5f / (ggt1 + ggt2);
}

float3 fresnel_schlick(float loh, float3 f0, float f90)
{
    return f0 + (f90.xxx - f0) * pow(1.0f - loh, 5.0f);
}

float diffusebrdf()
{
    // lambertian diffuse brdf needs to be energy conservative, that is where the pi term comes in
    // with this we can have diffuse colour be specified within range [0-1] and still satisfy energy conservation constraint
    // see https://www.rorydriscoll.com/2009/01/25/energy-conservation-in-games/
    return 1.0 / rt::pi;
}

float3 specularbrdf(float3 l, float3 v, float3 n, float r, float3 f0)
{
    float3 h = normalize(l + v);

    float noh = saturate(dot(n, h));
    float nov = saturate(dot(n, v));
    float nol = saturate(dot(n, l));
    float loh = saturate(dot(h, l));

    float r2 = r * r;
    float ndf = ggx_specularndf(noh, r2);
    float vis = ggx_specularvisibility(nov, nol, r2);
    float3 f = fresnel_schlick(loh, f0, 1.0f);
    
    return ndf * vis * f;
}

// irradiance is amount of light energy a surface recieves per unit area, at normal incidence
// assume all surfaces recieve same amount of light energy at normal incidence for now,
// this assumption holds well for directional lights which are assumed to be infinitely far away, but for point and spot lights we would need to attenuate the irradiance
float3 calculatelighting(float3 irradiance, float3 basecolour, float reflectance, float3 l, float3 v, float3 n, float metallic, float r)
{
    bool ismetallic = metallic > 0.001f;

    // metals do not have diffuse reflectance and non-metals have low specular reflectance
    float3 diffusealbedo = ismetallic ? (float3) 0.0f : basecolour;
    float3 sepcularalbedo = ismetallic ? basecolour : (float3) 0.04f; // constant low specular reflectance for dielectrics

    // dielectric specular reflectance is f0 = 0.16f * reflectance2
    // metallic specular reflectance is base colour
    // see Lagarde's "Moving Frostbite to PBR"
    float3 f0dielectric = (0.16f * reflectance * reflectance).xxx;
    float3 f0metallic = basecolour;

    float3 f0 = ismetallic ? f0metallic : f0dielectric;

    // map roughness from perceptually linear roughness
    r = r * r;
    float3 brdfs = specularbrdf(l, v, n, r, f0);
    float brdfd = diffusebrdf();
    
    float3 diffusecolour = brdfd * diffusealbedo;
    float3 specularcolour = brdfs * sepcularalbedo;
    float3 reflectedcolour = diffusecolour + specularcolour;

    float nol = saturate(dot(n, l));
    return irradiance * nol * reflectedcolour;
}

[shader("closesthit")]
void closesthitshader_triangle(inout raypayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    StructuredBuffer<float3> vertexbuffer = ResourceDescriptorHeap[4];
    StructuredBuffer<uint> indexbuffer = ResourceDescriptorHeap[5];
    ConstantBuffer<rt::sceneconstants> frameconstants = ResourceDescriptorHeap[rootconsts.frameidx];
    StructuredBuffer<uint> material_ids = ResourceDescriptorHeap[6];
    StructuredBuffer<rt::material> materials = ResourceDescriptorHeap[7];

    // index of blas in tlas, assume each instance only contains geometries that use same material id
    uint const geometryinstance_idx = InstanceIndex();

    rt::material geomaterial = materials[material_ids[geometryinstance_idx]];

    uint const triidx = PrimitiveIndex();
    
    float3 const v0 = vertexbuffer[indexbuffer[triidx * 3]];
    float3 const v1 = vertexbuffer[indexbuffer[triidx * 3 + 1]];
    float3 const v2 = vertexbuffer[indexbuffer[triidx * 3 + 2]];

    float3 const barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);    
    float3 const hitpoint = barycentrics.x * v0 + barycentrics.y * v1 + barycentrics.z * v2;

    // counter-clockwise winding
    float3 n = normalize(cross(v1 - v0, v2 - v0));
    float3 l = frameconstants.sundir;
    float3 v = normalize(frameconstants.campos - hitpoint);    
    float perceptual_r = geomaterial.roughness;
    float metallic = geomaterial.metallic;
    float4 basecolour = (float4)geomaterial.colour;
    float reflectance = geomaterial.reflectance;    
    
    payload.color.a = 1.0f;
    payload.color.xyz = calculatelighting(float3(50, 50, 50), basecolour.xyz, reflectance, l, v, n, metallic, perceptual_r);
}

#endif // RAYTRACE_HLSL