#pragma once

#include "sharedtypes.h"

#if __cplusplus

// import modules here
import stdxcore;
import vec;

#endif

namespace gfx
{

struct objdescriptors
{
    uint32 vertexbuffer;
    uint32 indexbuffer;
    uint32 objconstants;
    uint32 dispatchparams;
    uint32 materialid;
};

}