#ifndef PATHRACE_HLSL
#define PATHRACE_HLSL

#include "shaders/common.hlsli"
#include "shaders/brdf.hlsli"
#include "shared/raytracecommon.h"
#include "shaders/lighting.hlsli"

// todo : these are not on ptsettings as they are supposed to be removed eventually after being tested
static const bool randomlyoffsetcamerarays = false;
static const bool usehqrandoms = false;
static const bool userussianroulette = false;

#define use_ldsampler 0

ConstantBuffer<rt::rootdescs> rootdescriptors : register(b0);

enum sequencegen
{
    seqcameraray,
    seqindirectray,
    seqrussianroulette
};

struct raypayload
{
    // todo : remove radiance
    float3 radiance;
    float3 diffradiance;
    float3 specradiance;
    float3 thp;
    uint seed;
    uint currbounce;
};

struct ray
{
    float3 origin;
    float3 direction;
    uint idx;
};

struct shadingdata
{
    float3 colour;
    float2 mr;
    float3 normal;
    float3 geonormal;
};

float hashtofloat(uint v)
{
    return (v >> 8) / float(1 << 24);
}

uint murmurhash3(inout uint x)
{
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

uint hashcombine(uint seed, uint v)
{
    return seed ^ (murmurhash3(v) + 0x9e3779b9 + (v << 6) + (v >> 2));
}

uint hashbounce(uint2 pixel, uint bounce)
{
    // constant is golden ratio
    return hashcombine(bounce + 0x9e3779b9, pixel[0] << 16 | pixel[1]);
}

uint rayhash(uint bouncehash, uint sampleidx, sequencegen seqtype)
{
    return hashcombine(hashcombine(bouncehash, (uint)seqtype), sampleidx);
}

uint rayhash(uint2 pixel, uint sampleidx, sequencegen seqtype, uint bounce)
{
    return rayhash(hashbounce(pixel, bounce), sampleidx, seqtype);
}

// see nvidia rtxpt
// largely based on the functions there with functions names changed
uint owen_hash(uint x, uint seed)
{
     // this is from https://psychopath.io/post/2021_01_30_building_a_better_lk_hash (updated - thanks to https://github.com/CptLucky8 and https://pharr.org/matt/, see https://github.com/GameTechDev/XeGTAO/issues/7)
    x ^= x * 0x3d20adea;
    x += seed;
    x *= (seed >> 16) | 1;
    x ^= x * 0x05526c56;
    x ^= x * 0x53a22864;

    return x;
}

uint owen_scramble(uint x, uint seed) // nested_uniform_scramble_base2
{
    x = reversebits(x);
    x = owen_hash(x, seed);
    x = reversebits(x);
    return x;
}

uint sobol(uint index, uint dimension)
{
    const uint directions[5][32] = 
    {
        0x80000000, 0x40000000, 0x20000000, 0x10000000,
        0x08000000, 0x04000000, 0x02000000, 0x01000000,
        0x00800000, 0x00400000, 0x00200000, 0x00100000,
        0x00080000, 0x00040000, 0x00020000, 0x00010000,
        0x00008000, 0x00004000, 0x00002000, 0x00001000,
        0x00000800, 0x00000400, 0x00000200, 0x00000100,
        0x00000080, 0x00000040, 0x00000020, 0x00000010,
        0x00000008, 0x00000004, 0x00000002, 0x00000001,

        0x80000000, 0xc0000000, 0xa0000000, 0xf0000000,
        0x88000000, 0xcc000000, 0xaa000000, 0xff000000,
        0x80800000, 0xc0c00000, 0xa0a00000, 0xf0f00000,
        0x88880000, 0xcccc0000, 0xaaaa0000, 0xffff0000,
        0x80008000, 0xc000c000, 0xa000a000, 0xf000f000,
        0x88008800, 0xcc00cc00, 0xaa00aa00, 0xff00ff00,
        0x80808080, 0xc0c0c0c0, 0xa0a0a0a0, 0xf0f0f0f0,
        0x88888888, 0xcccccccc, 0xaaaaaaaa, 0xffffffff,

        0x80000000, 0xc0000000, 0x60000000, 0x90000000,
        0xe8000000, 0x5c000000, 0x8e000000, 0xc5000000,
        0x68800000, 0x9cc00000, 0xee600000, 0x55900000,
        0x80680000, 0xc09c0000, 0x60ee0000, 0x90550000,
        0xe8808000, 0x5cc0c000, 0x8e606000, 0xc5909000,
        0x6868e800, 0x9c9c5c00, 0xeeee8e00, 0x5555c500,
        0x8000e880, 0xc0005cc0, 0x60008e60, 0x9000c590,
        0xe8006868, 0x5c009c9c, 0x8e00eeee, 0xc5005555,

        0x80000000, 0xc0000000, 0x20000000, 0x50000000,
        0xf8000000, 0x74000000, 0xa2000000, 0x93000000,
        0xd8800000, 0x25400000, 0x59e00000, 0xe6d00000,
        0x78080000, 0xb40c0000, 0x82020000, 0xc3050000,
        0x208f8000, 0x51474000, 0xfbea2000, 0x75d93000,
        0xa0858800, 0x914e5400, 0xdbe79e00, 0x25db6d00,
        0x58800080, 0xe54000c0, 0x79e00020, 0xb6d00050,
        0x800800f8, 0xc00c0074, 0x200200a2, 0x50050093,

        0x80000000, 0x40000000, 0x20000000, 0xb0000000,
        0xf8000000, 0xdc000000, 0x7a000000, 0x9d000000,
        0x5a800000, 0x2fc00000, 0xa1600000, 0xf0b00000,
        0xda880000, 0x6fc40000, 0x81620000, 0x40bb0000,
        0x22878000, 0xb3c9c000, 0xfb65a000, 0xddb2d000,
        0x78022800, 0x9c0b3c00, 0x5a0fb600, 0x2d0ddb00,
        0xa2878080, 0xf3c9c040, 0xdb65a020, 0x6db2d0b0,
        0x800228f8, 0x400b3cdc, 0x200fb67a, 0xb00ddb9d,
    };

    uint x = 0u;
    [unroll] for (uint bit = 0; bit < 32; bit++)
    {
        uint mask = (index >> bit) & 1u;
        x ^= mask * directions[dimension][bit];
    }

    return x;
}

// sobol sampler
struct ldsampler
{
    static float4 generate(uint basehash, uint sampleidx, uint n)
    {
        float4 res;

        uint shuffleseed = hashcombine(basehash, 0);
        uint shuffledindex = owen_scramble(sampleidx, shuffleseed);

        [unroll] for(uint i = 0; i < min(n, 4); ++i)
		{
			uint dimseed = hashcombine(basehash, i + 1);
			uint dimsample = sobol(shuffledindex, i);
			dimsample = owen_scramble(dimsample, dimseed);
			res[i] = hashtofloat(dimsample);
		}

        return res;
    }
};

struct uniformsampler
{
    static float4 generate(uint basehash, uint sampleidx, uint n)
    {
        float4 res;
        [unroll] for (uint i = 0; i < min(n, 4); ++i)
            res[i] = hashtofloat(murmurhash3(basehash));

		return res;
    }
};

#if use_ldsampler
    #define seqsampler ldsampler
#else
    #define seqsampler uniformsampler
#endif

// see nvidia rtxpt

// taken from dxr tutorials https://intro-to-dxr.cwyman.org/
// renamed functions and variables

float rand(inout uint seed)
{
    seed = (1664525u * seed + 1013904223u);
    return float(seed & 0x00FFFFFF) / float(0x01000000);
}

// generates a seed for a random number generator from 2 inputs plus a backoff
uint initrand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

    [unroll] for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 coshemispheresample(float2 rands, float3 normal, bool usehqrandoms = false, float2 hqrandomvals = 0.xx)
{
    // get 2 random numbers to select our sample with
    float2 randomvals = usehqrandoms ? hqrandomvals : rands;

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

float2 wsvector_tolatlong(float3 dir)
{
    float3 p = normalize(dir);
    float u = (1.f + atan2(p.x, -p.z) * (1.0 / pi)) * 0.5f;
    float v = acos(p.y) * (1.0 / pi);
    return float2(u, v);
}
// taken from dxr tutorials https://intro-to-dxr.cwyman.org/

[shader("miss")]
void missshader(inout raypayload payload)
{
    if (payload.currbounce <= 1)
    {
        // for direct rays that miss sample sky texture
        StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];

        rt::dispatchparams params = paramsbuffer[0];

        StructuredBuffer<rt::sceneglobals> sceneglobals = ResourceDescriptorHeap[params.sceneglobals];
        Texture2D<float3> envtex = ResourceDescriptorHeap[sceneglobals[0].envtex];

        // todo : env map is broken
        // also pass dims
        float2 texdims;
        envtex.GetDimensions(texdims.x, texdims.y);

        float2 uv = wsvector_tolatlong(WorldRayDirection());
        payload.radiance = envtex[uint2(uv * texdims)];

        uint2 texel = DispatchRaysIndex().xy;

        uint idx = (sceneglobals[0].frameidx & 1u) ^ 1u;
        RWTexture2D<float4> difftex = ResourceDescriptorHeap[params.diffcolortex];
        RWTexture2D<float4> historylendepthdxtex = ResourceDescriptorHeap[params.historylentex[idx]];
        RWTexture2D<float4> specbrdfesttex = ResourceDescriptorHeap[params.specbrdftex];

        // clear textures on miss
        specbrdfesttex[texel] = 0.xxxx;
        difftex[texel] = 0.xxxx;

        // set high motion on miss so that reprojection always fails
        // we also don't need to clear hitposition and normal depth because they are only used if reprojection doens't fail
        float2 motion = float2(1e34f, 1e34f);
        historylendepthdxtex[texel].yzw = float3(0, motion);
    }
    else
        payload.radiance = float3(0, 0, 0);
}

// generate a ray in world space for a camera pixel from the dispatched 2D grid
ray generatecameraray(uint2 pixel, in float3 campos, float2 jitter, in float4x4 projtoworld)
{
    float2 xy = pixel + 0.5f + jitter;
    float2 screenpos = (xy / DispatchRaysDimensions().xy) * 2.0 - 1.0;

    // invert Y for DirectX-style coordinates.
    screenpos.y = -screenpos.y;

    // unproject the pixel coordinate into a world positon.
    float4 world = mul(projtoworld, float4(screenpos, 0, 1));
    world.xyz /= world.w;

    ray ray;
    ray.direction = normalize(world.xyz - campos);
    ray.origin = campos;

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

float2x3 dxdydz(float3x3 tri, float3 p, float3 campos, float4x4 viewproj, float4x4 invproj)
{
    float3 n = normalize(cross(tri[2] - tri[0], tri[1] - tri[0]));

    // this is only correct for primary rays
    //uint2 threadid = DispatchRaysIndex().xy;
    //ray ray0 = generatecameraray(uint2(threadid.x + 1, threadid.y), campos, jitter, invproj);
    //ray ray1 = generatecameraray(uint2(threadid.x, threadid.y + 1), campos, jitter, invproj);

    // project the hit point to ndc
    float4 ndc = mul(viewproj, float4(p, 1));
    ndc.xy = ndc.xy * rcp(ndc.w);

    float2 pixndcsize = 2.0 / DispatchRaysDimensions().xy;
    float2 ndcxoff = float2(ndc.x + pixndcsize.x, ndc.y);
    float2 ndcyoff = float2(ndc.x, ndc.y + pixndcsize.y);

    // unproject the pixel coordinate into a world positon.
    float4 world0 = mul(invproj, float4(ndcxoff, 0, 1));
    world0.xyz /= world0.w;

    float4 world1 = mul(invproj, float4(ndcyoff, 0, 1));
    world1.xyz /= world1.w;

    // helper rays
    ray ray0, ray1;
    ray0.origin = ray1.origin = campos;

    ray0.direction = normalize(world0.xyz - campos);
    ray1.direction = normalize(world1.xyz - campos);

    // intersect helper rays
    float3 xoffset = rayplaneintersect(p, n, ray0);
    float3 yoffset = rayplaneintersect(p, n, ray1);

    return float2x3(xoffset, yoffset);
}

float2x2 duv(float2x3 rayhits, float3x3 tri, float3x2 uvs, float2 uv)
{
    // compute barycentrics 
    float3 baryx = barycentriccoordinates(rayhits[0], tri[0], tri[1], tri[2]);
    float3 baryy = barycentriccoordinates(rayhits[1], tri[0], tri[1], tri[2]);

    // compute uvs and take the difference
    float2 duvx = mul(baryx, uvs) - uv;
    float2 duvy = mul(baryy, uvs) - uv;

    return float2x2(duvx, duvy);
}

shadingdata getshadingdata()
{
    // todo : move shading computation here
	return (shadingdata)0;
}

raypayload traceray(ray ray, uint currbounce, float3 thp, inout uint seed)
{
    StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams params = paramsbuffer[0];

    StructuredBuffer<rt::sceneglobals> scenedata = ResourceDescriptorHeap[params.sceneglobals];

    raypayload payload = (raypayload)0;
    payload.seed = seed;
    payload.currbounce = currbounce + 1;
    payload.thp = thp;
    if (currbounce >= scenedata[0].numbounces + 1)
    {
        payload.radiance = 0.xxx;
        return payload;
    }

    RayDesc raydesc;
    raydesc.Origin = ray.origin;
    raydesc.Direction = ray.direction;
    raydesc.TMin = 1e-3f;
    raydesc.TMax = 1e38;

    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[params.accelerationstruct];
    TraceRay(scene, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, raydesc, payload);
    seed = payload.seed;

    return payload;
}

[shader("raygeneration")]
void raygenshader()
{
    StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams params = paramsbuffer[0];

    StructuredBuffer<rt::viewglobals> viewbuf = ResourceDescriptorHeap[params.viewglobals];
    StructuredBuffer<rt::sceneglobals> sceneglobals = ResourceDescriptorHeap[params.sceneglobals];
    rt::viewglobals view = viewbuf[1];
    
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[params.accelerationstruct];
    StructuredBuffer<rt::ptsettings> ptsettingsbuffer = ResourceDescriptorHeap[params.ptsettings];

    uint sampleidx = sceneglobals[0].frameidx * ptsettingsbuffer[0].spp + rootdescriptors.sampleidx;

    float2 dxdy = seqsampler::generate(rayhash(DispatchRaysIndex().xy, sampleidx, seqcameraray, 0), sampleidx, 2).xy;

    // technically subpixeloffset can cause the sample to be located outside the pixel but this is fine?
    float2 const subpixeloffset = (randomlyoffsetcamerarays ? (dxdy - 0.5.xx) : 0.xx) + view.jitter;

    // generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    ray ray = generatecameraray(DispatchRaysIndex().xy, view.viewpos, subpixeloffset, view.invviewproj);

    uint seed = initrand(DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x, sceneglobals[0].frameidx, 16);

    // write to 1st texture on even frames
    uint idx = (sceneglobals[0].frameidx & 1u) ^ 1u;
    raypayload payload = traceray(ray, 0, 1.xxx, seed);
    RWTexture2D<float4> rendertarget = ResourceDescriptorHeap[params.rtoutput];
    RWTexture2D<float4> diffradiancetex = ResourceDescriptorHeap[params.diffradiancetex[idx]];
    RWTexture2D<float4> specradiancetex = ResourceDescriptorHeap[params.specradiancetex[idx]];

    uint2 const texel = DispatchRaysIndex().xy;

    // most path tracers do 1spp per frame, but we do all samples in same frame
    // todo : temp do not accumulate right now(assume 1spp)
    rendertarget[texel] = float4(any(isnan(payload.radiance)) ? 0.xxx : payload.radiance * rcp(ptsettingsbuffer[0].spp), 1.0);
    diffradiancetex[texel] = float4(any(isnan(payload.diffradiance)) ? 0.xxx : payload.diffradiance * rcp(ptsettingsbuffer[0].spp), 1.0f);
    specradiancetex[texel] = float4(any(isnan(payload.specradiance)) ? 0.xxx : payload.specradiance * rcp(ptsettingsbuffer[0].spp), 1.0f);
}

[shader("closesthit")]
void closesthitshader_triangle(inout raypayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams params = paramsbuffer[0];

    StructuredBuffer<rt::viewglobals> viewbuf = ResourceDescriptorHeap[params.viewglobals];
    StructuredBuffer<rt::sceneglobals> scene = ResourceDescriptorHeap[params.sceneglobals];

    StructuredBuffer<float3> posbuffer = ResourceDescriptorHeap[params.posbuffer];
    StructuredBuffer<index> indexbuffer = ResourceDescriptorHeap[params.indexbuffer];
    StructuredBuffer<uint> primmaterialsbuffer = ResourceDescriptorHeap[params.primitivematerialsbuffer];
    StructuredBuffer<material> materials = ResourceDescriptorHeap[scene[0].matbuffer];
    StructuredBuffer<float2> texcoords = ResourceDescriptorHeap[params.texcoordbuffer];
    StructuredBuffer<tbn> tbns = ResourceDescriptorHeap[params.tbnbuffer];
    
    RaytracingAccelerationStructure as = ResourceDescriptorHeap[params.accelerationstruct];

    rt::viewglobals prevview = viewbuf[0];
    rt::viewglobals view = viewbuf[1];

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

    // reorthonormalize?
    // todo : can use these as tangent basis for sampling specualr rays?
    //t = normalize((t - dot(t, n) * n));
    //b = normalize((b - dot(b, n) * n - dot(b, t) * t));

    material m = materials[primmaterialsbuffer[triidx]];

    Texture2D<float4> difftex = ResourceDescriptorHeap[NonUniformResourceIndex(m.diffusetex)];
    Texture2D<float4> roughnesstex = ResourceDescriptorHeap[NonUniformResourceIndex(m.roughnesstex)];
    Texture2D<float4> normaltex = ResourceDescriptorHeap[NonUniformResourceIndex(m.normaltex)];

    SamplerState sampler = SamplerDescriptorHeap[0];

    float3x3 tris = float3x3(p0, p1, p2);
    float2x3 rayhits = dxdydz(tris, p, view.viewpos, view.viewproj, view.invviewproj);
    float2x2 duvxy = duv(rayhits, tris, float3x2(uv0, uv1, uv2), uv);

    float3 normal = normaltex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]).xyz * 2.0 - 1.0;
    normal = t * normal.x + b * normal.y + n * normal.z;

    // todo : bug with normal maps so use geometry normal(possibly sampling below hemisphere causing nans)
    normal = n;

    float4 sampledcolour = difftex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]);
    float2 mr = roughnesstex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]).bg;
    float3 l = -scene[0].lightdir;
    float3 v = -WorldRayDirection();
    
    uint seed = payload.seed;

    RayDesc shadowraydesc;
    shadowraydesc.Origin = p;
    shadowraydesc.Direction = l;
    shadowraydesc.TMin = 1e-3f;
    shadowraydesc.TMax = 1e38;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;

    q.TraceRayInline(as, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, shadowraydesc);
    q.Proceed();
    
    float const nol = saturate(dot(normal, l));

    float3 const diffcolour = lerp(sampledcolour.rgb, 0.xxx, mr.x);
    float3 const speccolour = lerp(0.04.xxx, sampledcolour.rgb, mr.x);
    float const r = mr.y * mr.y;

    float3 const directirradiance = q.CommittedStatus() == COMMITTED_NOTHING ? scene[0].lightluminance * nol : 0.xxx;
    if (scene[0].enableao)
    {
        RayDesc aoray;
        aoray.Origin = p;
        aoray.Direction = coshemispheresample(seed, normal);
        aoray.TMin = 1e-3f;
        aoray.TMax = scene[0].aoradius;

        q.TraceRayInline(as, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, aoray);
        q.Proceed();

        float const aoval = q.CommittedStatus() == COMMITTED_NOTHING ? 0.0f : 1.0f;

        payload.seed = seed;
        payload.radiance = (1.0f - aoval).xxx;

        return;
    }

    float const lumdiffuse = max(0.01f, luminance(diffcolour.rgb));
    float const lumspecular = max(0.01f, luminance(speccolour.rgb));

    StructuredBuffer<rt::ptsettings> ptsettingsbuffer = ResourceDescriptorHeap[params.ptsettings];

    uint sampleidx = scene[0].frameidx * ptsettingsbuffer[0].spp + rootdescriptors.sampleidx;

    // todo : dispatch pixel subsamples from raygen instead of cpp?(with atomic add cpp dispatch could eliminate barriers)
    // todo : white furnance test
    // todo : use inline ray tracing
    // todo : add diffuse and sepcular transmissions, for transparent objects
    // todo : add delta reflections(is it really needed? maybe for mirrors??)
	uint baserayhash = hashcombine(hashbounce(DispatchRaysDimensions().xy, payload.currbounce), seqindirectray);
    float3 rayrands = seqsampler::generate(baserayhash, sampleidx, 3).xyz;

    // we have to decide whether we sample our diffuse or specular/ggx lobe
    // todo : does this need to be more involved if transmission is ignored
    float probdiffuse = lumdiffuse / (lumdiffuse + lumspecular);
    //bool choosediffuse = rayrands.z < probdiffuse;
    bool choosediffuse = rand(seed) < probdiffuse;
    float rcpprobdiff = rcp(probdiffuse);

    ggxsamplertype samplertype = (ggxsamplertype)scene[0].ggxsampleridx;

    ray ray;
    ray.origin = p;

    float2 randoms = float2(rand(seed), rand(seed));

	float3 currentthp = 0.xxx;
    if (choosediffuse)
    {
        ray.direction = coshemispheresample(randoms, normal, usehqrandoms, rayrands.xy);

        // thp = (costheta * brdf ) / (probdiffuse * costheta / pi)
        currentthp = diffcolour * rcpprobdiff;
    }
    else
    {
        ggxsampleeval res;
        if (samplertype == ggxsamplertype::vndf)
            res = eval<ggxvndf>(r, v, randoms, normal, speccolour);
        else if (samplertype == ggxsamplertype::bvndf)
            res = eval<ggxbvndf>(r, v, randoms, normal, speccolour);
        else
            res = eval<ggxndf>(r, v, randoms, normal, speccolour);

        currentthp = res.brdf / (1.0f - probdiffuse);
        ray.direction = res.dir;
    }

    payload.thp *= currentthp;

    // todo : normal we use here is normal mapped(is this correct?)
    // make sure our randomly selected, normal mapped ray didn't go below the surface
    // even without normal maps, not having this test creates darker results so keeping it
    bool terminatepath = dot(normal, ray.direction) <= 0.0f;
    if (userussianroulette)
    {
        float const rrprob = saturate(sqrt(luminance(currentthp)));
        uint rrhash = rayhash(baserayhash, sampleidx, seqrussianroulette);
        float const terminateprob = uniformsampler::generate(murmurhash3(rrhash), sampleidx, 1).x;

        // todo : we don't use payload.thp anywhere so rr doesn't work
        // always boost throughput due to roulette termination
        if (terminateprob > rrprob)
            terminatepath = true;
        else
            payload.thp *= rcp(rrprob);
    }

    float3 const directspecular = specularbrdf(l, v, normal, r, speccolour);

    payload.diffradiance += directirradiance * diffusebrdf().xxx * diffcolour;
    payload.specradiance += directirradiance * directspecular;

    float3 indirectcol = 0.xxx;
    if (!terminatepath)
    {
        raypayload indirectpayload = traceray(ray, payload.currbounce, payload.thp, seed);
        indirectcol = indirectpayload.radiance * currentthp;

        payload.diffradiance += choosediffuse ? indirectcol : 0.xxx;
        payload.specradiance += choosediffuse ? 0.xxx : indirectcol;
    }

    // store gbuffer data for camera ray hits of first sample
    if(payload.currbounce == 1 && rootdescriptors.sampleidx == 0)
    {
        uint idx = (scene[0].frameidx & 1u) ^ 1u;

        // todo : optimize these so they are packed tightly
        RWTexture2D<float4> normaldepthtex = ResourceDescriptorHeap[params.normaldepthtex[idx]];
        RWTexture2D<float4> difftex = ResourceDescriptorHeap[params.diffcolortex];
        RWTexture2D<float4> historylendepthdxtex = ResourceDescriptorHeap[params.historylentex[idx]];
        RWTexture2D<float4> hitpostex = ResourceDescriptorHeap[params.hitposition[idx]];
        RWTexture2D<float4> specbrdfesttex = ResourceDescriptorHeap[params.specbrdftex];

        float lineardepth = mul(view.view, float4(p, 1.0)).z;

        uint2 texel = DispatchRaysIndex().xy;

        // note : this is meant to work with ggx vndf
        float3 specbrdfestimate = approxspecularggxintegral(speccolour, r, saturate(dot(normal, v)));

        payload.diffradiance /= max(diffcolour, 1e-10);
        payload.specradiance /= max(specbrdfestimate, 1e-10);

        difftex[texel] = float4(diffcolour, 1.0);
        specbrdfesttex[texel] = float4(specbrdfestimate, 1.0);
        normaldepthtex[texel] = float4(n, lineardepth);
        hitpostex[texel] = float4(p, 0.0);

        float2 viewrayhitdepths = float2(mul(view.view, float4(rayhits[0], 1.0)).z, mul(view.view, float4(rayhits[1], 1.0)).z);

        // todo : this is planar derivative, so will not work for curved surfaces
        // todo : compute screen space finite difference derivatives
        float dxd = abs(viewrayhitdepths[0] - lineardepth) + abs(viewrayhitdepths[1] - lineardepth);

        // todo : pass dims
        float2 uv = float2(texel + 0.5) / float2(1920, 1080);
        float4 prevclip = mul(prevview.viewproj, float4(p, 1.0));
        float2 prevndc = prevclip.xy / prevclip.w;
        float2 prevuv = (prevndc / 2.0) + 0.5;
        prevuv.y = 1.0f - prevuv.y;

        float2 motionvector = (prevuv - uv);

        // todo : output dxd somewhere else
        historylendepthdxtex[texel] = float4(historylendepthdxtex[texel].x, dxd, motionvector.x, motionvector.y);
    }

    payload.seed = seed;
    payload.radiance = directirradiance * (directspecular + diffusebrdf() * diffcolour) + indirectcol;
}

#endif // PATHRACE_HLSL