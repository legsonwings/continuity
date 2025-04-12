#include "sphcommon.hlsli"

// todo : do this per vert?
#define TRIANGLES_PER_GROUP 85
#define THREADGROUP_X TRIANGLES_PER_GROUP

RWStructuredBuffer<float3> isosurface_vertices : register(u3);
RWStructuredBuffer<uint> isosurface_vertices_counter : register(u2);

[RootSignature(ROOTSIG_SPHGPU)]
[NumThreads(THREADGROUP_X, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint gid : SV_GroupID,
    uint gtid : SV_GroupThreadID,
    uint dtid : SV_DispatchThreadID,
    out indices uint3 tris[TRIANGLES_PER_GROUP],
    out vertices meshshadervertex verts[TRIANGLES_PER_GROUP * 3]
)
{
    uint const numprims = min((isosurface_vertices_counter[0] / 3u) - (gid * TRIANGLES_PER_GROUP), TRIANGLES_PER_GROUP);
    SetMeshOutputCounts(numprims * 3, numprims);

    meshshadervertex v0, v1, v2;
    
    if (dtid < numprims)
    {
        // the out buffers are local to group but input buffer is global
        uint v0idx = gtid * 3;
        uint v1idx = v0idx + 1;
        uint v2idx = v0idx + 2;

        tris[gtid] = uint3(v0idx, v1idx, v2idx);
        uint in_vertstart = (gid * TRIANGLES_PER_GROUP + gtid) * 3;

        v0.position = isosurface_vertices[in_vertstart];
        v1.position = isosurface_vertices[in_vertstart + 1];
        v2.position = isosurface_vertices[in_vertstart + 2];        

        float3 const trinormal = normalize(cross(v1.position - v0.position, v2.position - v1.position));

        v0.normal = trinormal;
        v1.normal = trinormal;
        v2.normal = trinormal;

        v0.positionh = mul(float4(v0.position, 1.0f), globals.viewproj);
        v1.positionh = mul(float4(v1.position, 1.0f), globals.viewproj);
        v2.positionh = mul(float4(v2.position, 1.0f), globals.viewproj);
        
        verts[v0idx] = v0;
        verts[v1idx] = v1;
        verts[v2idx] = v2;
    }
}
