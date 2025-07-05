#pragma once

#include "sharedtypes.h"

#if __cplusplus

// import modules here
import stdxcore;
import vec;

#endif

namespace rt
{

#if __cplusplus

struct alignas(256) sceneconstants
{
    stdx::vec3 campos;
    uint32 padding0;

    stdx::vec3 sundir;
    uint32 padding1;

    stdx::matrix4x4 inv_viewproj;
};

}
#else

struct sceneconstants
{
    float3 campos;
    uint32 padding0;
    
    float3 sundir;
    uint32 padding1;

    float4x4 inv_viewproj;
};

}

#endif