#include "common.hlsli"
#include "shared/common.h"

[NumThreads(ASGROUP_SIZE, 1, 1)]
void main(uint dtid : SV_DispatchThreadID, uint gtid : SV_GroupThreadID, uint gid : SV_GroupID)
{
    StructuredBuffer<gfx::objdescriptors> descriptors = ResourceDescriptorHeap[descriptorsidx.dispatchparams];

    uint const maxprimsperasg = descriptors[0].maxprims_permsgroup * ASGROUP_SIZE;
    uint const nummyprims = min(descriptors[0].numprims - gid * maxprimsperasg, maxprimsperasg);
    uint const nummsgroups = ceil(float(nummyprims) / float(descriptors[0].maxprims_permsgroup));

    payloaddata payload = (payloaddata)0;
    if (gtid < nummsgroups)
    {
        payload.data[gtid].start = gid * maxprimsperasg + gtid * descriptors[0].maxprims_permsgroup;
        payload.data[gtid].numprims = min(nummyprims - payload.data[gtid].start, descriptors[0].maxprims_permsgroup);
    }

    DispatchMesh(nummsgroups, 1, 1, payload);
}