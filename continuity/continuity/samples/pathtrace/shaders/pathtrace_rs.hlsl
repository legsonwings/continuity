#ifndef RAYTRACE_HLSL
#define RAYTRACE_HLSL

#include "shaders/common.hlsli"
#include "shared/raytracecommon.h"
#include "shaders/lighting.hlsli"

// params
static const bool randomsampleoffset = true;
static const bool usesuppliedrandoms = true;
static const bool userussianroulette = true;

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
    float3 radiance;
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

// returns a relative luminance of an input linear rgb color in the ITU-R BT.709 color space
float luminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

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

    [unroll] for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 coshemispheresample(inout uint seed, float3 normal, bool usesuppliedrandoms = false, float2 suppliedrandomvals = (float2)0)
{
    // get 2 random numbers to select our sample with
    float2 randomvals = usesuppliedrandoms ? suppliedrandomvals : float2(rand(seed), rand(seed));

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

// get a ggx half vector / microfacet normal, sampled according to the distribution computed by the function ggxnormaldistribution() above.  
// when using this function to sample, the probability density is pdf = d*g*f / (4*nol*nov)
float3 ggxmicrofacetn(inout uint randseed, float r, float3 hitnorm, bool usesuppliedrandoms = false, float2 suppliedrandomvals = (float2)0)
{
    // todo : does this produce vector in positive hemisphere only?
    // get our uniform random numbers
    float2 randval = usesuppliedrandoms ? suppliedrandomvals : float2(rand(randseed), rand(randseed));

    // get an orthonormal basis from the normal
    float3 b = perpendicular(hitnorm);
    float3 t = cross(b, hitnorm);

    // ggx ndf sampling
    float a2 = r * r;
    float costhetah = sqrt(max(0.0f, (1.0 - randval.x) / ((a2 - 1.0) * randval.x + 1)));
    float sinthetah = sqrt(max(0.0f, 1.0f - costhetah * costhetah));
    float phih = randval.y * pi * 2.0f;

    // get our ggx ndf sample (i.e., the half vector)
    return t * (sinthetah * cos(phih)) + b * (sinthetah * sin(phih)) + hitnorm * costhetah;
}
// taken from dxr tutorials https://intro-to-dxr.cwyman.org/

[shader("miss")]
void missshader(inout raypayload payload)
{
    if (payload.currbounce <= 1)
    {
        // for direct rays that miss sample sky texture
        StructuredBuffer<rt::dispatchparams> descriptorsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
        rt::dispatchparams descriptors = descriptorsbuffer[0];

        StructuredBuffer<rt::sceneglobals> sceneglobals = ResourceDescriptorHeap[descriptors.sceneglobals];
        Texture2D<float3> envtex = ResourceDescriptorHeap[sceneglobals[0].envtex];

        float2 texdims;
        envtex.GetDimensions(texdims.x, texdims.y);

        float2 uv = wsvector_tolatlong(WorldRayDirection());
        payload.radiance = envtex[uint2(uv * texdims)];
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

float2x2 duv(float3x3 tri, float3x2 uvs, float3 p, float2 uv, float3 campos, float4x4 viewproj, float4x4 invproj)
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

    // compute barycentrics 
    float3 baryx = barycentriccoordinates(xoffset, tri[0], tri[1], tri[2]);
    float3 baryy = barycentriccoordinates(yoffset, tri[0], tri[1], tri[2]);

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

    // todo : assumes 16 spp
    uint sampleidx = sceneglobals[0].frameidx * 16u + rootdescriptors.sampleidx;

    float2 dxdy = seqsampler::generate(rayhash(DispatchRaysIndex().xy, sampleidx, seqcameraray, 0), sampleidx, 2).xy;

    // technically subpixeloffset can cause the sample to be located outside the pixel but this is fine?
    float2 const subpixeloffset = (randomsampleoffset ? (dxdy - 0.5.xx) : (float2)0) + view.jitter;

    // generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    ray ray = generatecameraray(DispatchRaysIndex().xy, view.viewpos, subpixeloffset, view.invviewproj);

    uint seed = initrand(DispatchRaysIndex().x + DispatchRaysIndex().y * DispatchRaysDimensions().x, sceneglobals[0].frameidx, 16);

    raypayload payload = traceray(ray, 0, seed);
    RWTexture2D<float4> rendertarget = ResourceDescriptorHeap[descriptors.rtoutput];

    bool colorsnan = any(isnan(payload.radiance));

    // write the raytraced color to the output texture
    rendertarget[DispatchRaysIndex().xy] = float4(colorsnan ? 0.xxx : payload.radiance / 16u, 1.0);
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
    
    RaytracingAccelerationStructure as = ResourceDescriptorHeap[descriptors.accelerationstruct];

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

    float2x2 duvxy = duv(float3x3(p0, p1, p2), float3x2(uv0, uv1, uv2), p, uv, view.viewpos, view.viewproj, view.invviewproj);

    float3 normal = normaltex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]).xyz * 2.0 - 1.0;
    normal = t * normal.x + b * normal.y + n * normal.z;

    float4 sampledcolour = difftex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]);
    float2 mr = roughnesstex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]).bg;
    float3 l = -scene[0].lightdir;
    float3 v = normalize(view.viewpos - p);
    
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
    float3 const speccolour = lerp(0.04f.xxx, sampledcolour.rgb, mr.x);

    float const nov = saturate(dot(normal, v));
    float const r = mr.y * mr.y;

    float3 directcolour = (float3)0;
    if (q.CommittedStatus() == COMMITTED_NOTHING)
    {
        float3 const specular = specularbrdf(l, v, normal, r, speccolour);
        directcolour = scene[0].lightluminance * nol * (specular + diffusebrdf() * diffcolour);
    }

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

    // todo : assumes 16 spp
    uint sampleidx = scene[0].frameidx * 16u + rootdescriptors.sampleidx;
    
    // todo : white furnance test
    // todo : rejecting indirect direction sample below surfaces will create a bias, so better to just sample positive hemisphere
    //        sample vndf to get less variance for ggx specular, diffuse probably has someting similar in rtxpt
    // todo : use inline ray tracing
    // todo : add diffuse and sepcular transmissions, for transparent objects
    // todo : add delta reflections(is it really needed? maybe for mirrors??)

	uint baserayhash = hashcombine(hashbounce(DispatchRaysDimensions().xy, payload.currbounce), seqindirectray);
    float3 rayrands = seqsampler::generate(baserayhash, sampleidx, 3).xyz;

    // we have to decide whether we sample our diffuse or specular/ggx lobe
    // todo : does this need to be more involved if transmission is ignored
    float probdiffuse = lumdiffuse / (lumdiffuse + lumspecular);
    float choosediffuse = rayrands.z < probdiffuse;

    ray ray;
    ray.origin = p;

	float3 currentthp = (float3)0;
    float3 indirectcol = (float3)0;
    if (choosediffuse)
    {
        ray.direction = coshemispheresample(seed, normal, usesuppliedrandoms, rayrands.xy);

        // thp = (costheta * brdf ) / (probdiffuse * costheta / pi)
        currentthp = diffcolour * rcp(probdiffuse);
    }
    else
    {
        // randomly sample the ndf to get a microfacet in our brdf to reflect off of
        // then compute the outgoing direction based on this (perfectly reflective) microfacet
        float3 const spech = ggxmicrofacetn(seed, r, normal, usesuppliedrandoms, rayrands.xy);
        float3 const specdir = normalize(2.f * dot(v, spech) * spech - v);
        
        ray.direction = specdir;

        float const loh = saturate(dot(specdir, spech));
        float const noh = saturate(dot(normal, spech));
        float const nol = saturate(dot(normal, specdir));

        float const r2 = r * r;
        float const ndf = ggx_specularndf(noh, r2);
        float const vis = ggx_specularvisibility(nov, nol, r2);
        float3 const f = fresnel_schlick(loh, speccolour, 1.0f);

        float3 const specular = ndf * vis * f;

        float const ndfprob = ndf * noh / (4 * loh);

        // throughput : ggx-brdf * nol / probability-of-sampling
        currentthp = nol * specular / (ndfprob * (1.0f - probdiffuse));
    }

    payload.thp *= currentthp;

    // make sure our randomly selected, normal mapped ray didn't go below the surface
	// always boost throughput due to roulette termination
    bool terminatepath = dot(n, ray.direction) <= 0.0f;
    if (userussianroulette)
    {
        uint rrhash = rayhash(baserayhash, sampleidx, seqrussianroulette);
        float const rrprob = sqrt(luminance(payload.thp));

        if (uniformsampler::generate(murmurhash3(rrhash), sampleidx, 1).x > rrprob)
            terminatepath = true;
        else
            payload.thp *= rcp(rrprob);
    }

    if (!terminatepath)
    {
        raypayload indirectpayload = traceray(ray, payload.currbounce, seed);

        indirectcol = indirectpayload.radiance * currentthp;
    }

    payload.seed = seed;
    payload.radiance = (directcolour + indirectcol) * rcp(16);
}

#endif // RAYTRACE_HLSL