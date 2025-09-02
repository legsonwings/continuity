#include "common.hlsli"
#include "shared/common.h"

//StructuredBuffer<vertexin> triangle_verts : register(t0);

meshshadervertex getvertattribute(vertexin vertex)
{
    StructuredBuffer<gfx::dispatchparams> descriptors = ResourceDescriptorHeap[descriptorsidx.dispatchparams];
    StructuredBuffer<instance_data> objconstants = ResourceDescriptorHeap[descriptors[0].objconstants];

    meshshadervertex outvert;

    float4 const pos = float4(vertex.position, 1.f);
    outvert.position = mul(objconstants[0].matx, pos).xyz;
    //outvert.positionh = mul(objconstants[0].mvpmatx, pos);
    outvert.normal = normalize(mul(objconstants[0].normalmatx, float4(vertex.normal, 0)).xyz);
    
    return outvert;
}

#define MAX_VERTICES_PER_GROUP (MAX_TRIANGLES_PER_GROUP * 3)

[NumThreads(128, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload payloaddata payload,
    out indices uint3 tris[MAX_TRIANGLES_PER_GROUP],
    out vertices meshshadervertex verts[MAX_VERTICES_PER_GROUP]
)
{
    const uint numprims = payload.data[gid].numprims;
    SetMeshOutputCounts(numprims * 3, numprims);

    if (gtid < numprims)
    {
        // The out buffers are local to group but input buffer is global
        int v0idx = gtid * 3;
        int v1idx = v0idx + 1;
        int v2idx = v0idx + 2;

        tris[gtid] = uint3(v0idx, v1idx, v2idx);
        int in_vertstart = (payload.data[gid].start + gtid) * 3;
     
        //StructuredBuffer<gfx::dispatchparams> descriptors = ResourceDescriptorHeap[descriptorsidx.objdescriptors];
        //StructuredBuffer<vertexin> triangle_verts = ResourceDescriptorHeap[descriptors[0].vertexbuffer];

        //verts[v0idx] = getvertattribute(triangle_verts[in_vertstart]);
        //verts[v1idx] = getvertattribute(triangle_verts[in_vertstart + 1]);
        //verts[v2idx] = getvertattribute(triangle_verts[in_vertstart + 2]);

        meshshadervertex dummy = (meshshadervertex)0;
        verts[v0idx] = dummy;
        verts[v1idx] = dummy;
        verts[v2idx] = dummy;
    }
}
