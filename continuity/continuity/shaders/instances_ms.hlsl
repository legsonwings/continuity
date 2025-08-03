#include "common.hlsli"
#include "shared/common.h"

meshshadervertex getvertattribute(vertexin vertex, uint vertidx)
{
    meshshadervertex outvert;

    StructuredBuffer<gfx::objdescriptors> descriptors = ResourceDescriptorHeap[descriptorsidx.objdescriptors];
    StructuredBuffer<instance_data> objconstants = ResourceDescriptorHeap[descriptors[0].objconstants];
    StructuredBuffer<uint> primmaterials = ResourceDescriptorHeap[objconstants[0].primmaterialsidx];
    StructuredBuffer<sceneglobals> sceneglobals = ResourceDescriptorHeap[descriptorsidx.sceneglobals];
    StructuredBuffer<viewconstants> viewglobals = ResourceDescriptorHeap[descriptorsidx.viewglobals];

    float4 const pos = float4(vertex.position, 1.f);
    float4 const posw = mul(pos, objconstants[0].matx);
    outvert.instanceid = 0;
    outvert.position = posw.xyz;
    outvert.positionh = mul(posw, viewglobals[0].viewproj);
    outvert.texcoords = vertex.texcoord;
    outvert.material = primmaterials[vertidx / 3];

    // world space vectors
    outvert.normal = normalize(mul(float4(vertex.normal, 0), objconstants[0].normalmatx).xyz);
    outvert.tangent = normalize(mul(float4(vertex.tangent, 0), objconstants[0].matx).xyz);
    outvert.bitangent = normalize(mul(float4(vertex.bitangent, 0), objconstants[0].matx).xyz);

    // light's view projection matrix at index 1
    outvert.positionl = mul(posw, viewglobals[1].viewproj);

    return outvert;
}

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
    out vertices meshshadervertex verts[MAX_VERTICES_PER_GROUP]
)
{
    StructuredBuffer<gfx::objdescriptors> descriptors = ResourceDescriptorHeap[descriptorsidx.objdescriptors];
    StructuredBuffer<dispatch_parameters> dispatch_params = ResourceDescriptorHeap[descriptors[0].dispatchparams];
    StructuredBuffer<float3> triangle_positions = ResourceDescriptorHeap[descriptors[0].posbuffer];
    StructuredBuffer<float2> triangle_texcoords = ResourceDescriptorHeap[descriptors[0].texcoordbuffer];
    StructuredBuffer<tbn> triangle_tbns = ResourceDescriptorHeap[descriptors[0].tbnbuffer];
    StructuredBuffer<index> triangle_indices = ResourceDescriptorHeap[descriptors[0].indexbuffer];

    uint const numprims = min(dispatch_params[0].numprims - gid * MAX_PRIMS_PER_GROUP, MAX_PRIMS_PER_GROUP);
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

        v0.texcoord = triangle_texcoords[i0.texcoord];
        v1.texcoord = triangle_texcoords[i1.texcoord];
        v2.texcoord = triangle_texcoords[i2.texcoord];

        v0.normal = triangle_tbns[i0.tbn].normal;
        v1.normal = triangle_tbns[i1.tbn].normal;
        v2.normal = triangle_tbns[i2.tbn].normal;

        v0.tangent = triangle_tbns[i0.tbn].tangent;
        v1.tangent = triangle_tbns[i1.tbn].tangent;
        v2.tangent = triangle_tbns[i2.tbn].tangent;

        v0.bitangent = triangle_tbns[i0.tbn].bitangent;
        v1.bitangent = triangle_tbns[i1.tbn].bitangent;
        v2.bitangent = triangle_tbns[i2.tbn].bitangent;

        verts[v0idx] = getvertattribute(v0, dtid * 3u);
        verts[v0idx + 1] = getvertattribute(v1, dtid * 3u + 1);
        verts[v0idx + 2] = getvertattribute(v2, dtid * 3u + 2);
    }
}
