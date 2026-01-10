module;

#include "thirdparty/d3dx12.h"

module graphics:renderpasses;

namespace gfx
{

void uavbarrierpass::operator()(deviceresources& devres)
{
    devres.cmdlist->ResourceBarrier(UINT(barriers.size()), barriers.data());
}

}