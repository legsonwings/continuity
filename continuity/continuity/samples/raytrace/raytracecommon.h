#pragma once

#if __cplusplus

// import modules here
import stdxcore;
import vec;

#endif

namespace rt
{

#if __cplusplus

struct rootconstants
{
    uint32 frameidx;
};

struct alignas(256) sceneconstants
{
    stdx::vec3 campos;
    uint8 padding0[4];
    stdx::matrix4x4 inv_viewproj;
};

}
#else

struct rootconstants
{
    uint frameidx;
};

struct sceneconstants
{
    float3 campos;
    uint padding0;
    float4x4 inv_viewproj;
};

}

#endif