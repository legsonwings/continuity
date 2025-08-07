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
    uint32 numprims;
    uint32 numverts_perprim;
    uint32 maxprims_permsgroup;
    uint32 numprims_perinstance;
    uint32 indexbuffer;
    uint32 posbuffer;
    uint32 texcoordbuffer;
    uint32 tbnbuffer;
    uint32 objconstants;
    uint32 materialid;
};

}