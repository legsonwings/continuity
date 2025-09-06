#ifndef RAYTRACE_HLSL
#define RAYTRACE_HLSL

#include "shaders/common.hlsli"
#include "shared/raytracecommon.h"
#include "shaders/lighting.hlsli"

ConstantBuffer<rt::rootdescs> rootdescriptors : register(b0);

struct raypayload
{
    float3 radiance;
    uint seed;
    uint currbounce;
};

struct ray
{
    float3 origin;
    float3 direction;
};

// taken from dxr tutorials https://intro-to-dxr.cwyman.org/
// renamed functions and variables
float rand(inout uint seed)
{
    seed = (1664525u * seed + 1013904223u);
    return float(seed & 0x00FFFFFF) / float(0x01000000);
}

// from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 perpendicular(float3 u)
{
    float3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, float3(xm, ym, zm));
}

// generates a seed for a random number generator from 2 inputs plus a backoff
uint initrand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

    [unroll]
    for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 coshemispheresample(inout uint seed, float3 normal)
{
    // get 2 random numbers to select our sample with
    float2 randomvals = float2(rand(seed), rand(seed));

    // cosine weighted hemisphere sample from RNG
    float3 bitangent = perpendicular(normal);
    float3 tangent = cross(bitangent, normal);
    float r = sqrt(randomvals.x);
    float phi = 2.0f * 3.14159265f * randomvals.y;

    // get our cosine-weighted hemisphere lobe sample direction
    return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + normal.xyz * sqrt(max(0.0, 1.0f - randomvals.x));
}

float3 uniformhemispheresample(inout uint seed, float3 normal)
{
    // get 2 random numbers to select our sample with
    float2 randomvals = float2(rand(seed), rand(seed));

    // uniform hemisphere sample from RNG
    float3 bitangent = perpendicular(normal);
    float3 tangent = cross(bitangent, normal);
    float r = sqrt(max(0.0f, 1.0f - randomvals.x * randomvals.x));
    float phi = 2.0f * 3.14159265f * randomvals.y;

    // get our uniform sample direction
    return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + normal.xyz * randomvals.x;
}
// taken from dxr tutorials https://intro-to-dxr.cwyman.org/

[shader("miss")]
void missshader(inout raypayload payload)
{
    payload.radiance = float3(0, 0, 0);
}

// generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
ray generatecameraray(uint2 index, in float3 campos, in float4x4 projtoworld)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // unproject the pixel coordinate into a world positon.
    float4 world = mul(projtoworld, float4(screenPos, 0, 1));
    world.xyz /= world.w;

    ray ray;
    ray.direction = normalize(world.xyz - campos);

    // add bias to prevent self-intersections
    ray.origin = campos + ray.direction * 1e-4;

    return ray;
}

float3 rayplaneintersect(float3 planeorigin, float3 n, ray ray)
{
    float t = dot(-n, ray.origin - planeorigin) / dot(n, ray.direction);
    return ray.origin + ray.direction * t;
}

float3 barycentriccoordinates(float3 pt, float3 v0, float3 v1, float3 v2)
{
    // see https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
    // from "Real-Time Collision Detection" by Christer Ericson
    float3 e0 = v1 - v0;
    float3 e1 = v2 - v0;
    float3 e2 = pt - v0;
    float d00 = dot(e0, e0);
    float d01 = dot(e0, e1);
    float d11 = dot(e1, e1);
    float d20 = dot(e2, e0);
    float d21 = dot(e2, e1);
    float denom = 1.0 / (d00 * d11 - d01 * d01);
    float v = (d11 * d20 - d01 * d21) * denom;
    float w = (d00 * d21 - d01 * d20) * denom;
    float u = 1.0 - v - w;
    return float3(u, v, w);
}

float2x2 duv(float3x3 tri, float3x2 uvs, float3 p, float2 uv, float3 campos, float4x4 invproj)
{
    float3 n = normalize(cross(tri[2] - tri[0], tri[1] - tri[0]));

    // helper rays
    uint2 threadid = DispatchRaysIndex().xy;
    ray ray0 = generatecameraray(uint2(threadid.x + 1, threadid.y), campos, invproj);
    ray ray1 = generatecameraray(uint2(threadid.x, threadid.y + 1), campos, invproj);

    // intersect helper rays
    float3 xoffset = rayplaneintersect(p, n, ray0);
    float3 yoffset = rayplaneintersect(p, n, ray1);

    // compute barycentrics 
    float3 baryx = barycentriccoordinates(xoffset, tri[0], tri[1], tri[2]);
    float3 baryy = barycentriccoordinates(yoffset, tri[0], tri[1], tri[2]);

    // compute uvs and take the difference
    float2 duvx = mul(baryx, uvs) - uv;
    float2 duvy = mul(baryy, uvs) - uv;

    return float2x2(duvx, duvy);
}

raypayload traceray(ray ray, uint currbounce, inout uint seed)
{
    StructuredBuffer<rt::dispatchparams> descriptorsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams descriptors = descriptorsbuffer[0];

    StructuredBuffer<rt::sceneglobals> scenedata = ResourceDescriptorHeap[descriptors.sceneglobals];

    raypayload payload = (raypayload)0;
    payload.seed = seed;
    payload.currbounce = currbounce + 1;
    if (currbounce >= scenedata[0].numbounces + 1)
    {
        payload.radiance = (float3)0.0;
        return payload;
    }

    RayDesc raydesc;
    raydesc.Origin = ray.origin;
    raydesc.Direction = ray.direction;
    raydesc.TMin = 1e-3f;
    raydesc.TMax = 1e38;

    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[descriptors.accelerationstruct];
    TraceRay(scene, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, raydesc, payload);
    seed = payload.seed;

    return payload;
}

[shader("raygeneration")]
void raygenshader()
{
    StructuredBuffer<rt::dispatchparams> descriptorsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams descriptors = descriptorsbuffer[0];

    StructuredBuffer<rt::viewglobals> viewbuf = ResourceDescriptorHeap[descriptors.viewglobals];
    StructuredBuffer<rt::sceneglobals> sceneglobals = ResourceDescriptorHeap[descriptors.sceneglobals];
    rt::viewglobals view = viewbuf[0];
    
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[descriptors.accelerationstruct];

    // generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    ray ray = generatecameraray(DispatchRaysIndex().xy, view.viewpos, view.invviewproj);

    uint seed = initrand(DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x, sceneglobals[0].frameidx, 16);

    raypayload payload = traceray(ray, 0, seed);
    RWTexture2D<float4> rendertarget = ResourceDescriptorHeap[descriptors.rtoutput];

    // write the raytraced color to the output texture
    rendertarget[DispatchRaysIndex().xy] = float4(payload.radiance, 1.0);
}

[shader("closesthit")]
void closesthitshader_triangle(inout raypayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    StructuredBuffer<rt::dispatchparams> descriptorsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams descriptors = descriptorsbuffer[0];

    StructuredBuffer<rt::viewglobals> viewbuf = ResourceDescriptorHeap[descriptors.viewglobals];
    StructuredBuffer<rt::sceneglobals> scene = ResourceDescriptorHeap[descriptors.sceneglobals];

    StructuredBuffer<float3> posbuffer = ResourceDescriptorHeap[descriptors.posbuffer];
    StructuredBuffer<index> indexbuffer = ResourceDescriptorHeap[descriptors.indexbuffer];
    StructuredBuffer<uint> primmaterialsbuffer = ResourceDescriptorHeap[descriptors.primitivematerialsbuffer];
    StructuredBuffer<material> materials = ResourceDescriptorHeap[scene[0].matbuffer];
    StructuredBuffer<float2> texcoords = ResourceDescriptorHeap[descriptors.texcoordbuffer];
    StructuredBuffer<tbn> tbns = ResourceDescriptorHeap[descriptors.tbnbuffer];

    rt::viewglobals view = viewbuf[0];

    uint const triidx = PrimitiveIndex();

    float3 const barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    index const i0 = indexbuffer[triidx * 3];
    index const i1 = indexbuffer[triidx * 3 + 1];
    index const i2 = indexbuffer[triidx * 3 + 2];

    float3 const p0 = posbuffer[i0.pos];
    float3 const p1 = posbuffer[i1.pos];
    float3 const p2 = posbuffer[i2.pos];
    float3 const p = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

    float2 const uv0 = texcoords[i0.texcoord];
    float2 const uv1 = texcoords[i1.texcoord];
    float2 const uv2 = texcoords[i2.texcoord];
    float2 const uv = barycentrics.x * uv0 + barycentrics.y * uv1 + barycentrics.z * uv2;

    // todo : transform these vectors to world space using blas trasnform
    float3 const t0 = tbns[i0.tbn].tangent;
    float3 const t1 = tbns[i1.tbn].tangent;
    float3 const t2 = tbns[i2.tbn].tangent;
    float3 const b0 = tbns[i0.tbn].bitangent;
    float3 const b1 = tbns[i1.tbn].bitangent;
    float3 const b2 = tbns[i2.tbn].bitangent;
    float3 const n0 = tbns[i0.tbn].normal;
    float3 const n1 = tbns[i1.tbn].normal;
    float3 const n2 = tbns[i2.tbn].normal;
    float3 const t = normalize(barycentrics.x * t0 + barycentrics.y * t1 + barycentrics.z * t2);
    float3 const b = normalize(barycentrics.x * b0 + barycentrics.y * b1 + barycentrics.z * b2);
    float3 const n = normalize(barycentrics.x * n0 + barycentrics.y * n1 + barycentrics.z * n2);

    material m = materials[primmaterialsbuffer[triidx]];

    Texture2D<float4> difftex = ResourceDescriptorHeap[NonUniformResourceIndex(m.diffusetex)];
    Texture2D<float4> roughnesstex = ResourceDescriptorHeap[NonUniformResourceIndex(m.roughnesstex)];
    Texture2D<float4> normaltex = ResourceDescriptorHeap[NonUniformResourceIndex(m.normaltex)];

    SamplerState sampler = SamplerDescriptorHeap[0];
    SamplerState pointsampler = SamplerDescriptorHeap[1];

    float2x2 duvxy = duv(float3x3(p0, p1, p2), float3x2(uv0, uv1, uv2), p, uv, view.viewpos, view.invviewproj);

    float3 normal = normaltex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]).xyz * 2.0 - 1.0;
    normal = t * normal.x + b * normal.y + n * normal.z;

    float4 sampledcolour = difftex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]);
    float2 mr = roughnesstex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]).bg;
    float3 l = -scene[0].lightdir;
    float3 v = normalize(view.viewpos - p);
    
    RayDesc shadowraydesc;
    shadowraydesc.Origin = p;
    shadowraydesc.Direction = l;
    shadowraydesc.TMin = 1e-3f;
    shadowraydesc.TMax = 1e38;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;

    RaytracingAccelerationStructure as = ResourceDescriptorHeap[descriptors.accelerationstruct];

    q.TraceRayInline(as, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, shadowraydesc);
    q.Proceed();

    float3 directdiffuse = (float3)0;
    if (q.CommittedStatus() == COMMITTED_NOTHING)
        directdiffuse = scene[0].lightluminance;

    float const nol = saturate(dot(normal, l));

    uint seed = payload.seed;

    float3 indirectdiffuse = (float3)0;
    for (uint i = 0; i < scene[0].numindirectrays; ++i)
    {
        float3 dir = coshemispheresample(seed, normal);

        ray ray;
        ray.origin = p;
        ray.direction = dir;

        // todo : the seed is probably overwritten by the last ray's seed which isn't great?
        raypayload indirectpayload = traceray(ray, payload.currbounce, seed);
        indirectdiffuse += indirectpayload.radiance;
    }

    indirectdiffuse /= scene[0].numindirectrays;

    payload.seed = seed;
    payload.radiance += (directdiffuse / pi + 2 * indirectdiffuse) * nol * sampledcolour.rgb;
}

#endif // RAYTRACE_HLSL