#include "sphcommon.hlsli"

#define TRIANGLES_PER_GROUP 85
#define THREADGROUP_X TRIANGLES_PER_GROUP

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
    uint const numprims = min(sph_dispatch_params.numparticles - gid * THREADGROUP_X, THREADGROUP_X);
    SetMeshOutputCounts(numprims * 3, numprims);

    meshshadervertex v0, v1, v2;
    
    float const triradius = 0.25f;
    float3 const center = particledata[dtid].p;

    float4 v0p = float4(center + float3(triradius * float2(0.0f, 1.0f), 0.0f), 1.0f);
    float4 v1p = float4(center + float3(triradius * float2(0.866f, -0.5f), 0.0f), 1.0f);
    float4 v2p = float4(center + float3(triradius * float2(-0.866f, -0.5f), 0.0f), 1.0f);

    v0.position = v0p.xyz;
    v1.position = v1p.xyz;
    v2.position = v2p.xyz;
    v0.positionh = mul(v0p, globals.viewproj);
    v1.positionh = mul(v1p, globals.viewproj);
    v2.positionh = mul(v2p, globals.viewproj);
   
    v0.normal = float3(0.0, 0.0, 1.0);
    v1.normal = float3(0.0, 0.0, 1.0);
    v2.normal = float3(0.0, 0.0, 1.0);
    
    // The out buffers are local to group but input buffer is global
    int v0idx = gtid * 3;
    int v1idx = v0idx + 1;
    int v2idx = v0idx + 2;

    tris[ gtid] = 
    
    tris[gtid] = uint3(v0idx, v1idx, v2idx);

    verts[v0idx] = v0;
    verts[v1idx] = v1;
    verts[v2idx] = v2;
}
