#include "common.hlsli"
#include "shared/common.h"

meshshadervertex getvertattribute(vertexin vertex, float3 n)
{
    meshshadervertex outvert;
    
    StructuredBuffer<gfx::objdescriptors> descriptors = ResourceDescriptorHeap[descriptorsidx.objdescriptors];
    StructuredBuffer<object_constants> objconstants = ResourceDescriptorHeap[descriptors[0].objconstants];

    float4 const pos = float4(vertex.position, 1.f);
    outvert.instanceid = 0;
    outvert.position = mul(pos, objconstants[0].matx).xyz;
    outvert.positionh = mul(pos, objconstants[0].mvpmatx);

    // world space face normal(todo : vertex normals aren't passed in yet)
    outvert.normal = normalize(mul(float4(n, 0), objconstants[0].normalmatx).xyz);

    return outvert;
}

// only output 128 to simplify things
#define MAX_PRIMS_PER_GROUP 85
#define MAX_VERTICES_PER_GROUP 255

[RootSignature(ROOT_SIG)]
[NumThreads(85, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint dtid : SV_DispatchThreadID,
    uint gtid : SV_GroupThreadID,
    uint gid : SV_GroupID,
    in payload payloaddata payload,
    out indices uint3 tris[MAX_PRIMS_PER_GROUP],
    out vertices meshshadervertex verts[MAX_VERTICES_PER_GROUP]
)
{
    StructuredBuffer<gfx::objdescriptors> descriptors = ResourceDescriptorHeap[descriptorsidx.objdescriptors];
    StructuredBuffer<dispatch_parameters> dispatch_params = ResourceDescriptorHeap[descriptors[0].dispatchparams];
    StructuredBuffer<vertexin> triangle_vertices = ResourceDescriptorHeap[descriptors[0].vertexbuffer];
    StructuredBuffer<uint> triangle_indices = ResourceDescriptorHeap[descriptors[0].indexbuffer];

    uint const numprims = min(dispatch_params[0].numprims - gid * MAX_PRIMS_PER_GROUP, MAX_PRIMS_PER_GROUP);
    SetMeshOutputCounts(numprims * 3, numprims);

    if (gtid < numprims)
    {
        uint const v0idx = gtid * 3u;

        tris[gtid] = uint3(v0idx, v0idx + 1, v0idx + 2);

        // the out buffers are local to group but input buffer is global
        // not very optimal as this is writing duplicate vertices
        vertexin v0 = triangle_vertices[triangle_indices[dtid * 3u]];
        vertexin v1 = triangle_vertices[triangle_indices[dtid * 3u + 1]];
        vertexin v2 = triangle_vertices[triangle_indices[dtid * 3u + 2]];

        // counter-clockwise
        float3 n = normalize(cross(v1.position - v0.position, v2.position - v0.position));
        verts[v0idx] = getvertattribute(v0, n);
        verts[v0idx + 1] = getvertattribute(v1, n);
        verts[v0idx + 2] = getvertattribute(v2, n);
    }
}
