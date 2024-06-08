#include "sphcommon.hlsli"

// asume extents are for cube
float3 getparticle_position(uint particleidx, float3 containerorigin,  float3 extents, uint numparticles)
{
    // todo : store in global memory
    uint const gridwidth = ceil(pow(numparticles, 0.33333333f));
    float3 const origin = containerorigin - (extents.x / 2);
    
    // avoid division by zero, by returing same cell size of one and two particles
    float const cellsize = extents.x / max(gridwidth - 1, 1);
    
    uint3 const xyz = uint3(particleidx % gridwidth, (particleidx / gridwidth) % gridwidth, particleidx / (gridwidth * gridwidth));
    return origin + (xyz * cellsize);
}

[RootSignature(ROOTSIG_SPHGPU)]
[numthreads(64, 1, 1)]
void main(uint dtid : SV_DispatchThreadID)
{
    particledata[dtid].p = getparticle_position(dtid, sph_dispatch_params.containerorigin, sph_dispatch_params.containerextents, sph_dispatch_params.numparticles);
}