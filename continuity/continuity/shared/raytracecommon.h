#pragma once

#include "sharedtypes.h"

#if __cplusplus

// import modules here
import stdxcore;

#endif

namespace rt
{

struct sceneglobals
{
    uint32 matbuffer;
    float3 lightpos0;
    float3 lightdir0;
    float lightluminance0;
    float3 lightpos1;
    float3 lightdir1;
    float lightluminance1;
    uint32 frameidx;
    uint32 seedu;
    float seed;

    // settings
    uint32 viewdirshading;
    uint32 viewmode;
    uint32 numbounces;
    uint32 numindirectrays;
};

struct viewglobals
{
    float3 viewpos;
    float4x4 viewproj;
    float4x4 invviewproj;
};

struct rootdescs
{
    uint32 rootdesc;
};

struct dispatchparams
{
    uint32 accelerationstruct;
    uint32 rtoutput;
    uint32 indexbuffer;
    uint32 primitivematerialsbuffer;
    uint32 posbuffer;
    uint32 texcoordbuffer;
    uint32 tbnbuffer;
    uint32 sceneglobals;
    uint32 viewglobals;
};

}