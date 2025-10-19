#pragma once

#include "sharedtypes.h"

enum ptmodes
{
    ptref = 0,
    pthighquality = 1,
    ptrealtime = 2
};

#define staticconst static const

#if __cplusplus

// import modules here
import stdxcore;

#undef staticconst

#define staticconst static constexpr

#endif

enum class ggxsamplertype : uint32
{
    ndf = 0,
    vndf,
    bvndf
};

namespace rt
{

staticconst ptmodes ptmode = ptrealtime;

struct ptsettings
{
    uint32 spp;

    // flags
    uint32 simpleaccum;
    uint32 camjitter;
};

staticconst ptsettings ptmodesettings[] =
{
    { 1u, 1u, 0u },
    { 1u, 0u, 0u },
    { 1u, 0u, 0u }
};

struct accumparams
{
    uint32 currtexture;
    uint32 hdrcolourtex;
    uint32 accumcount;
    uint32 ptsettings;
    uint32 viewglobals;
    uint32 sceneglobals;
    uint2 hitposition;
    uint2 diffradiance;
    uint2 specradiance;
    uint2 normaldepthtex;
    uint2 historylentex;
    uint2 momentstex;
    uint2 dims;
    float alpha;
    float alphamom;
};

struct denoiserootconsts
{
	uint32 params;
	uint32 passidx;
};

struct denoiserparams
{
    uint2 difftex;
    uint2 spectex;
    uint2 normaldepthtex;
    uint2 historylentex;
    uint2 hitposition;
	uint2 dims;
    uint32 sceneglobals;
};

struct postdenoiseparams
{
    uint32 diffcolor;
    uint32 specbrdf;
    uint2 diffradiance;
    uint2 specradiance;
    uint32 outputrt;
    uint32 sceneglobals;
};

struct sceneglobals
{
    uint32 matbuffer;
    uint32 frameidx;
    uint32 envtex;
    float3 lightdir;
    float lightluminance;
    float seed;

    // settings
    uint32 viewdirshading;
    uint32 viewmode;
    uint32 ggxsampleridx;
    uint32 numbounces;
    uint32 enableao;
    float aoradius;
};

struct viewglobals
{
    float2 jitter;
    float3 viewpos;
    float4x4 view;
    float4x4 viewproj;
    float4x4 invviewproj;
};

struct rootdescs
{
    uint32 rootdesc;
    uint32 sampleidx;
    uint32 passidx;
};

struct dispatchparams
{
    uint32 accelerationstruct;
    uint2 normaldepthtex;
    uint2 historylentex;
    uint2 hitposition;
    uint32 diffcolortex;
    uint32 specbrdftex;
    uint2 diffradiancetex;
    uint2 specradiancetex;
    uint32 rtoutput;
    uint32 indexbuffer;
    uint32 primitivematerialsbuffer;
    uint32 posbuffer;
    uint32 texcoordbuffer;
    uint32 tbnbuffer;
    uint32 sceneglobals;
    uint32 viewglobals;
	uint32 ptsettings;
};

}