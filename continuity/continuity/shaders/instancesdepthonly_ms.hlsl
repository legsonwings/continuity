#include "common.hlsli"
#include "shared/common.h"

struct outvert
{
    float4 positionh : SV_Position;
};

#define MAX_PRIMS_PER_GROUP 85
#define MAX_VERTICES_PER_GROUP MAX_PRIMS_PER_GROUP * 3

[numthreads(85, 1, 1)]
[outputtopology("triangle")]
void main
(
    uint dtid : SV_DispatchThreadID,
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    out indices uint3 tris[MAX_PRIMS_PER_GROUP],
    out vertices outvert verts[MAX_VERTICES_PER_GROUP]
)
{
    StructuredBuffer<gfx::dispatchparams> descriptors = ResourceDescriptorHeap[descriptorsidx.dispatchparams];
    StructuredBuffer<float3> triangle_positions = ResourceDescriptorHeap[descriptors[0].posbuffer];
    StructuredBuffer<index> triangle_indices = ResourceDescriptorHeap[descriptors[0].indexbuffer];
    StructuredBuffer<instance_data> objconstants = ResourceDescriptorHeap[descriptors[0].objconstants];
    StructuredBuffer<viewconstants> viewglobals = ResourceDescriptorHeap[descriptorsidx.viewglobals];

    uint const numprims = min(descriptors[0].numprims - gid * MAX_PRIMS_PER_GROUP, MAX_PRIMS_PER_GROUP);
    SetMeshOutputCounts(numprims * 3, numprims);

    if (gtid < numprims)
    {
        uint const v0idx = gtid * 3u;

        tris[gtid] = uint3(v0idx, v0idx + 1, v0idx + 2);

        index i0 = triangle_indices[dtid * 3u];
        index i1 = triangle_indices[dtid * 3u + 1];
        index i2 = triangle_indices[dtid * 3u + 2];

        // the out buffers are local to group but input buffer is global
        // not very optimal as this is writing duplicate vertices, but the restriction according to specs is to
        // export all vertices in a group from the same group
        vertexin v0, v1, v2;

        v0.position = triangle_positions[i0.pos];
        v1.position = triangle_positions[i1.pos];
        v2.position = triangle_positions[i2.pos];

        outvert outv0, outv1, outv2;

        // ideally should be getting mvp from cpu
        float4x4 mvp = mul(objconstants[0].matx, viewglobals[1].viewproj);

        outv0.positionh = mul(mvp, float4(v0.position, 1.0f));
        outv1.positionh = mul(mvp, float4(v1.position, 1.0f));
        outv2.positionh = mul(mvp, float4(v2.position, 1.0f));

        verts[v0idx] = outv0;
        verts[v0idx + 1] = outv1;
        verts[v0idx + 2] = outv2;
    }
}
