#ifndef PATHRACE_HLSL
#define PATHRACE_HLSL

#include "shaders/common.hlsli"
#include "shaders/brdf.hlsli"
#include "shared/raytracecommon.h"
#include "shaders/lighting.hlsli"

// todo : these are not on ptsettings as they are supposed to be removed eventually after being tested
static const bool userussianroulette = false;

ConstantBuffer<rt::rootdescs> rootdescriptors : register(b0);

enum sequencegen
{
    seqcameraray,
    seqindirectray,
    seqrussianroulette
};

enum class raytype
{
    camera,
    diffusebounce,
    specularbounce
};

struct ray
{
    float3 origin;
    float3 direction;
    raytype type;
};

struct hitdata
{
    float2 uv;
    float3 position;
    float3 normal;
    float2x2 duvxy;
    float2x3 rayhits;
    uint triidx;
    bool hit;
};

struct shadingdata
{
    float3 normal;
    float3 diffcolour;
    float3 speccolour;

    // linear roughness
    float r;
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

uint murmurhash3rw(inout uint x)
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
        basehash = hashcombine(basehash, sampleidx);

        float4 res;
        [unroll] for (uint i = 0; i < min(n, 4); ++i)
            res[i] = hashtofloat(murmurhash3rw(basehash));

		return res;
    }
};

// see nvidia rtxpt

// taken from dxr tutorials https://intro-to-dxr.cwyman.org/
// renamed functions and variables

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 coshemispheresample(float2 rands, float3 normal)
{
    // cosine weighted hemisphere sample from RNG
    float3 bitangent = perpendicular(normal);
    float3 tangent = cross(bitangent, normal);
    float r = sqrt(rands.x);
    float phi = 2.0f * 3.14159265f * rands.y;

    // get our cosine-weighted hemisphere lobe sample direction
    return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + normal.xyz * sqrt(max(0.0, 1.0f - rands.x));
}

float3 uniformhemispheresample(float2 rands, float3 normal)
{
    // get 2 random numbers to select our sample with
    float2 randomvals = rands;

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

// generate a ray in world space for a camera pixel from the dispatched 2D grid
ray generatecameraray(uint2 pixel, in float3 campos, float2 jitter, in float4x4 projtoworld)
{
    // untested : radomly offset camera rays
    {
        //uint sampleidx = scene.frameidx * ptsettingsbuffer[0].spp + rootdescriptors.sampleidx;
        //float2 dxdy = ldsampler::generate(rayhash(pixel, sampleidx, seqcameraray, 0), sampleidx, 2).xy;

        //static const bool randomlyoffsetcamerarays = false;
        ////technically subpixeloffset can cause the sample to be located outside the pixel but this is fine?
        //jitter += (randomlyoffsetcamerarays ? (dxdy - 0.5.xx) : 0.xx);
    }

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
    ray.type = raytype::camera;

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

hitdata gethitdata(float2 barycentrics, uint triidx)
{
    float3 bary;
    bary.yz = barycentrics;
    bary.x = (1 - bary.y - bary.z);

    StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams params = paramsbuffer[0];

    StructuredBuffer<float3> posbuffer = ResourceDescriptorHeap[params.posbuffer];
    StructuredBuffer<index> indexbuffer = ResourceDescriptorHeap[params.indexbuffer];
    StructuredBuffer<tbn> tbns = ResourceDescriptorHeap[params.tbnbuffer];
    StructuredBuffer<float2> texcoords = ResourceDescriptorHeap[params.texcoordbuffer];
    StructuredBuffer<rt::viewglobals> viewbuf = ResourceDescriptorHeap[params.viewglobals];

    rt::viewglobals view = viewbuf[1];

    index const i0 = indexbuffer[triidx * 3];
    index const i1 = indexbuffer[triidx * 3 + 1];
    index const i2 = indexbuffer[triidx * 3 + 2];

    float3 const p0 = posbuffer[i0.pos];
    float3 const p1 = posbuffer[i1.pos];
    float3 const p2 = posbuffer[i2.pos];
    float3 const p = bary.x * p0 + bary.y * p1 + bary.z * p2;

    float2 const uv0 = texcoords[i0.texcoord];
    float2 const uv1 = texcoords[i1.texcoord];
    float2 const uv2 = texcoords[i2.texcoord];
    float2 const uv = bary.x * uv0 + bary.y * uv1 + bary.z * uv2;

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
    float3 const t = normalize(bary.x * t0 + bary.y * t1 + bary.z * t2);
    float3 const b = normalize(bary.x * b0 + bary.y * b1 + bary.z * b2);
    float3 const n = normalize(bary.x * n0 + bary.y * n1 + bary.z * n2);

    // reorthonormalize?
    // todo : can use these as tangent basis for sampling specualr rays?
    //t = normalize((t - dot(t, n) * n));
    //b = normalize((b - dot(b, n) * n - dot(b, t) * t));

    float3x3 tris = float3x3(p0, p1, p2);
    float2x3 rayhits = dxdydz(tris, p, view.viewpos, view.viewproj, view.invviewproj);
    float2x2 duvxy = duv(rayhits, tris, float3x2(uv0, uv1, uv2), uv);

    hitdata res;
    res.triidx = triidx;
    res.uv = uv;
    res.duvxy = duvxy;
    res.rayhits = rayhits;
    res.position = p;
    res.normal = n;
    res.hit = true;

    return res;
}

shadingdata getshadingdata(hitdata trihit)
{
    StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams params = paramsbuffer[0];

    StructuredBuffer<rt::sceneglobals> scene = ResourceDescriptorHeap[params.sceneglobals];
    StructuredBuffer<uint> primmaterialsbuffer = ResourceDescriptorHeap[params.primitivematerialsbuffer];
    StructuredBuffer<material> materials = ResourceDescriptorHeap[scene[0].matbuffer];

    material m = materials[primmaterialsbuffer[trihit.triidx]];

    Texture2D<float4> difftex = ResourceDescriptorHeap[NonUniformResourceIndex(m.diffusetex)];
    Texture2D<float4> roughnesstex = ResourceDescriptorHeap[NonUniformResourceIndex(m.roughnesstex)];
    Texture2D<float4> normaltex = ResourceDescriptorHeap[NonUniformResourceIndex(m.normaltex)];

    SamplerState sampler = SamplerDescriptorHeap[0];

    //float3 normal = normaltex.SampleGrad(sampler, uv, duvxy[0], duvxy[1]).xyz * 2.0 - 1.0;
    //normal = t * trihit.normal.x + b * trihit.normal.y + n * trihit.normal.z;

    // todo : bug with normal maps so use geometry normal(possibly sampling below hemisphere causing nans)
    float3 normal = trihit.normal;

    float4 sampledcolour = m.colour;
    if(m.diffusetex != ~0)
        sampledcolour = difftex.SampleGrad(sampler, trihit.uv, trihit.duvxy[0], trihit.duvxy[1]);
    
    float2 mr = float2(m.metallic, m.roughness);
    if (m.roughnesstex != ~0)
        mr = roughnesstex.SampleGrad(sampler, trihit.uv, trihit.duvxy[0], trihit.duvxy[1]).bg;

    float3 const diffcolour = lerp(sampledcolour.rgb, 0.xxx, mr.x);
    float3 const speccolour = lerp(0.04.xxx, sampledcolour.rgb, mr.x);

    shadingdata res;
    res.normal = normal;
    res.diffcolour = diffcolour;
    res.speccolour = speccolour;
    res.r = mr.y * mr.y;

    return res;
}

struct bouncedata
{
    ray ray;
    uint baserayhash;
    float3 currthp;
};

bouncedata bounceray(float3 p, uint2 pixel, float r, float3 v, uint currbounce, uint sampleidx, float3 normal, float3 diffc, float3 specc, uint ggxsampleridx)
{
    bouncedata res;

    float const lumdiffuse = max(0.01f, luminance(diffc));
    float const lumspecular = max(0.01f, luminance(specc));
    res.baserayhash = hashbounce(pixel, currbounce);
    float3 rayrands = ldsampler::generate(res.baserayhash, sampleidx, 3).xyz;

    // we have to decide whether we sample our diffuse or specular/ggx lobe
    // todo : does this need to be more involved if transmission is ignored
    float probdiffuse = lumdiffuse / (lumdiffuse + lumspecular);
    bool isdiffuse = rayrands.z < probdiffuse;
    float rcpprobdiff = rcp(probdiffuse);

    ggxsamplertype samplertype = (ggxsamplertype)ggxsampleridx;

    float2 randoms = rayrands.xy;

    res.ray.origin = p;
    if (isdiffuse)
    {
        res.ray.type = raytype::diffusebounce;
        res.ray.direction = coshemispheresample(randoms, normal);

        // thp = (costheta * brdf ) / (probdiffuse * costheta / pi)
        res.currthp = diffc * rcpprobdiff;
    }
    else
    {
        res.ray.type = raytype::specularbounce;

        ggxsampleeval evalres;
        if (samplertype == ggxsamplertype::vndf)
            evalres = eval<ggxvndf>(r, v, randoms, normal, specc);
        else if (samplertype == ggxsamplertype::bvndf)
            evalres = eval<ggxbvndf>(r, v, randoms, normal, specc);
        else
            evalres = eval<ggxndf>(r, v, randoms, normal, specc);

        res.currthp = evalres.brdf / (1.0f - probdiffuse);
        res.ray.direction = evalres.dir;
    }

    return res;
}

hitdata traceray(ray ray, RaytracingAccelerationStructure as)
{
    hitdata res = (hitdata)0;

    RayDesc raydesc;
    raydesc.Origin = ray.origin;
    raydesc.Direction = ray.direction;
    raydesc.TMin = 1e-3f;
    raydesc.TMax = 1e38;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_CULL_FRONT_FACING_TRIANGLES | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
    q.TraceRayInline(as, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, raydesc);
    q.Proceed();

    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        res = gethitdata(q.CommittedTriangleBarycentrics(), q.CommittedPrimitiveIndex());

    return res;
}

float2x3 directradiance(hitdata hit, shadingdata shade, float3 l, float3 v, float3 thp, float3 lum, RaytracingAccelerationStructure as)
{
    float3 p = hit.position;
    float3 normal = shade.normal;

    RayDesc shadowraydesc;
    shadowraydesc.Origin = p;
    shadowraydesc.Direction = l;
    shadowraydesc.TMin = 1e-3f;
    shadowraydesc.TMax = 1e38;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;

    q.TraceRayInline(as, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, shadowraydesc);
    q.Proceed();

    float const nol = saturate(dot(normal, l));

    float3 const directirradiance = q.CommittedStatus() == COMMITTED_NOTHING ? lum * nol * thp : 0.xxx;
    float3 const directdiff = directirradiance * diffusebrdf() * shade.diffcolour;
    float3 const directspec = directirradiance * specularbrdf(l, v, normal, shade.r, shade.speccolour);

    return float2x3(directdiff, directspec);
}

void storegbufferdata(hitdata hit, float3 diffcolour, float3 specbrdfest)
{
    StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams params = paramsbuffer[0];

    StructuredBuffer<rt::viewglobals> viewbuf = ResourceDescriptorHeap[params.viewglobals];
    StructuredBuffer<rt::sceneglobals> sceneglobalsbuf = ResourceDescriptorHeap[params.sceneglobals];

    rt::viewglobals prevview = viewbuf[0];
    rt::viewglobals view = viewbuf[1];
    rt::sceneglobals scene = sceneglobalsbuf[0];

    // store gbuffer data for camera ray hits
    uint idx = (scene.frameidx & 1u) ^ 1u;

    // todo : optimize these so they are packed tightly
    RWTexture2D<float4> normaldepthtex = ResourceDescriptorHeap[params.normaldepthtex[idx]];
    RWTexture2D<float4> historylendepthdxtex = ResourceDescriptorHeap[params.historylentex[idx]];
    RWTexture2D<float4> hitpostex = ResourceDescriptorHeap[params.hitposition[idx]];
    RWTexture2D<float4> difftex = ResourceDescriptorHeap[params.diffcolortex];
    RWTexture2D<float4> specbrdfesttex = ResourceDescriptorHeap[params.specbrdftex];

    uint2 const texel = DispatchRaysIndex().xy;

    float lineardepth = mul(view.view, float4(hit.position, 1.0)).z;
    float2 viewrayhitdepths = float2(mul(view.view, float4(hit.rayhits[0], 1.0)).z, mul(view.view, float4(hit.rayhits[1], 1.0)).z);

    // todo : this is planar derivative, so will not work for curved surfaces
    // todo : compute screen space finite difference derivatives
    float dxd = abs(viewrayhitdepths[0] - lineardepth) + abs(viewrayhitdepths[1] - lineardepth);

    // todo : pass dims
    float2 uv = float2(texel + 0.5) / float2(params.dims);
    float4 prevclip = mul(prevview.viewproj, float4(hit.position, 1.0));
    float2 prevndc = prevclip.xy / prevclip.w;
    float2 prevuv = (prevndc / 2.0) + 0.5;
    prevuv.y = 1.0f - prevuv.y;

    float2 motionvector = (prevuv - uv);

    difftex[texel] = float4(diffcolour, 1.0);
    specbrdfesttex[texel] = float4(specbrdfest, 1.0);
    normaldepthtex[texel] = float4(hit.normal, lineardepth);
    hitpostex[texel] = float4(hit.position, 0.0);
    // todo : output dxd somewhere else
    historylendepthdxtex[texel] = float4(historylendepthdxtex[texel].x, dxd, motionvector.x, motionvector.y);
}

bool russianroulette(inout bouncedata bouncedray, uint sampleidx)
{
    if (userussianroulette)
    {
        float const rrprob = saturate(sqrt(luminance(bouncedray.currthp)));
        uint rrhash = rayhash(bouncedray.baserayhash, sampleidx, seqrussianroulette);
        float const terminateprob = uniformsampler::generate(murmurhash3(rrhash), sampleidx, 1).x;

        // todo :test rr
        // always boost throughput due to roulette termination
        if (terminateprob > rrprob)
            return true;
        else
            bouncedray.currthp *= rcp(rrprob);

        return false;
    }

    return false;
}

float3 cameraraymiss(float3 raydir)
{
    // for camera rays that miss sample sky texture
    StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];

    rt::dispatchparams params = paramsbuffer[0];

    StructuredBuffer<rt::sceneglobals> sceneglobals = ResourceDescriptorHeap[params.sceneglobals];
    Texture2D<float3> envtex = ResourceDescriptorHeap[sceneglobals[0].envtex];

    uint2 texel = DispatchRaysIndex().xy;
    uint idx = (sceneglobals[0].frameidx & 1u) ^ 1u;
    RWTexture2D<float4> difftex = ResourceDescriptorHeap[params.diffcolortex];
    RWTexture2D<float4> historylendepthdxtex = ResourceDescriptorHeap[params.historylentex[idx]];
    RWTexture2D<float4> specbrdfesttex = ResourceDescriptorHeap[params.specbrdftex];
    RWTexture2D<float4> normaldepthtex = ResourceDescriptorHeap[params.normaldepthtex[idx]];

    // write gbuffer on miss
    {
        // on miss set diffuse radiance to env map radiance and specular radiance to 0
        difftex[texel] = 1.xxxx;
        specbrdfesttex[texel] = 0.xxxx;
        normaldepthtex[texel] = float4(0, 0, -1, 1e34);

        // set high motion on miss so that reprojection always fails
        // we also don't need to clear hitposition as it is only used if reprojection doesn't fail and it doesn't cause issues in spatial pass
        float2 motion = float2(1e34f, 1e34f);
        historylendepthdxtex[texel].yzw = float3(0, motion);
    }

    float2 texdims;
    envtex.GetDimensions(texdims.x, texdims.y);

    float2 uv = wsvector_tolatlong(raydir);
    return envtex[uint2(uv * texdims)];
}

void writeradiance(uint2 texel, float3 rad, float3 diffrad, float3 specrad)
{
    StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams params = paramsbuffer[0];

    StructuredBuffer<rt::sceneglobals> sceneglobalsbuf = ResourceDescriptorHeap[params.sceneglobals];

    rt::sceneglobals scene = sceneglobalsbuf[0];

    StructuredBuffer<rt::ptsettings> ptsettingsbuffer = ResourceDescriptorHeap[params.ptsettings];

    // write to 1st texture on even frames
    uint idx = (scene.frameidx & 1u) ^ 1u;

    bool const refmode = ptsettingsbuffer[0].simpleaccum;

    if (refmode)
    {
        RWTexture2D<float4> rendertarget = ResourceDescriptorHeap[params.rtoutput];
        rendertarget[texel] = float4(any(isnan(rad)) ? 0.xxx : rad * rcp(ptsettingsbuffer[0].spp), 1.0); // ref mode
    }
    else
    {
        RWTexture2D<float4> diffradiancetex = ResourceDescriptorHeap[params.diffradiancetex[idx]];
        RWTexture2D<float4> specradiancetex = ResourceDescriptorHeap[params.specradiancetex[idx]];
        diffradiancetex[texel] = float4(any(isnan(diffrad)) ? 0.xxx : diffrad * rcp(ptsettingsbuffer[0].spp), 1.0f);
        specradiancetex[texel] = float4(any(isnan(specrad)) ? 0.xxx : specrad * rcp(ptsettingsbuffer[0].spp), 1.0f);
    }
}

[shader("raygeneration")]
void raygenshader()
{
    // todo : white furnance test
    // todo : test russian roulette
    // todo : add diffuse and sepcular transmissions, for transparent objects
    // todo : add delta reflections(is it really needed? maybe for mirrors??)

    StructuredBuffer<rt::dispatchparams> paramsbuffer = ResourceDescriptorHeap[rootdescriptors.rootdesc];
    rt::dispatchparams params = paramsbuffer[0];

    StructuredBuffer<rt::viewglobals> viewbuf = ResourceDescriptorHeap[params.viewglobals];
    StructuredBuffer<rt::sceneglobals> sceneglobalsbuf = ResourceDescriptorHeap[params.sceneglobals];

    rt::viewglobals prevview = viewbuf[0];
    rt::viewglobals view = viewbuf[1];
    rt::sceneglobals scene = sceneglobalsbuf[0];

    RaytracingAccelerationStructure as = ResourceDescriptorHeap[params.accelerationstruct];
    StructuredBuffer<rt::ptsettings> ptsettingsbuffer = ResourceDescriptorHeap[params.ptsettings];

    uint2 const texel = DispatchRaysIndex().xy;

    float3 radiance, diffradiance, specradiance;
    radiance = diffradiance = specradiance = 0.xxx;

    float3 l = -scene.lightdir;

    ray firstbounce;
    ray nextray = generatecameraray(texel, view.viewpos, 0.xx, view.invviewproj);
    hitdata camrayhit = traceray(nextray, as);
    if (camrayhit.hit)
    {
        float3 v = -nextray.direction;
        shadingdata camshade = getshadingdata(camrayhit);

        // note : this is meant to work with ggx vndf
        float3 specbrdfest = approxspecularggxintegral(camshade.speccolour, camshade.r, saturate(dot(camrayhit.normal, v)));
        storegbufferdata(camrayhit, camshade.diffcolour, specbrdfest);

        float2x3 directrad = directradiance(camrayhit, camshade, l, v, 1.xxx, scene.lightluminance, as);

        diffradiance = directrad[0];
        specradiance = directrad[1];

        uint sampleidx = scene.frameidx * ptsettingsbuffer[0].spp + rootdescriptors.sampleidx;

        bouncedata bouncedray = bounceray(camrayhit.position, texel, camshade.r, v, 0, sampleidx, camshade.normal, camshade.diffcolour, camshade.speccolour, scene.ggxsampleridx);
        firstbounce = bouncedray.ray;

        nextray = firstbounce;
        float3 accumiradiance = 0.xxx;
        float3 thp = bouncedray.currthp;

        // do not enter the bounce loop if next ray is below the hemisphere
        // even without normal maps, not having these tests creates slightly darker results so keeping it
        // todo : figure out if it matters much
        uint bouncecount = dot(camrayhit.normal, nextray.direction) <= 0.0 ? 0xFFFFFFFF : 1;
        while (bouncecount <= scene.numbounces)
        {
            hitdata irayhit = traceray(nextray, as);
            bool terminatepath = !irayhit.hit;
            if (!terminatepath)
            {
                float3 iv = -nextray.direction;
                shadingdata ishade = getshadingdata(irayhit);
                float2x3 idirectrad = directradiance(irayhit, ishade, l, iv, thp, scene.lightluminance, as);
                accumiradiance += idirectrad[0] + idirectrad[1];

                bouncedata ibouncedray = bounceray(irayhit.position, texel, ishade.r, iv, bouncecount, sampleidx, irayhit.normal, ishade.diffcolour, ishade.speccolour, scene.ggxsampleridx);
                nextray = ibouncedray.ray;
                thp *= ibouncedray.currthp;

                terminatepath = dot(irayhit.normal, nextray.direction) <= 0.0f;
                terminatepath = terminatepath || russianroulette(ibouncedray, sampleidx);
            }

            bouncecount++;
            if (terminatepath) break;
        }

        // for ref mode
        radiance = diffradiance + specradiance + accumiradiance;

        if (firstbounce.type == raytype::diffusebounce)
            diffradiance += accumiradiance;
        else if (firstbounce.type == raytype::specularbounce)
            specradiance += accumiradiance;

        diffradiance /= max(camshade.diffcolour, 1e-10);
        specradiance /= max(specbrdfest, 1e-10);
    }
    else
    {
        radiance = cameraraymiss(nextray.direction);
        diffradiance = radiance;
    }

    writeradiance(texel, radiance, diffradiance, specradiance);
}

#endif // PATHRACE_HLSL