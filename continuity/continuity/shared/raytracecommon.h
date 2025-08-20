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
    uint32 viewdirshading;
    float3 lightdir;
    float lightluminance;
};

struct viewglobals
{
    float3 viewpos;
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