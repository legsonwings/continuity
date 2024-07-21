#include "sphcommon.hlsli"

[RootSignature(ROOTSIG_SPHGPU)]
[NumThreads(64, 1, 1)]
void main(uint dtid : SV_DispatchThreadID)
{
    particledata[dtid].rho = 0.0f;
    for (uint i = 0; i < sph_dispatch_params.numparticles; ++i)
    {
        float3 diff = particledata[dtid].p - particledata[i].p;
        float distsqr = dot(diff, diff);
        
        if (distsqr < sph_dispatch_params.hsqr)
        {
            particledata[dtid].rho += sph_dispatch_params.poly6coeff * pow(sph_dispatch_params.hsqr - distsqr, 3u);
        }
    }
 
    particledata[dtid].rho = max(particledata[dtid].rho, sph_dispatch_params.rho0);
    particledata[dtid].pr = sph_dispatch_params.k * (particledata[dtid].rho - sph_dispatch_params.rho0);
    
    if(dtid == 0)
    {
        // reset vertices count here and render dispatch args
        isosurface_vertices_counter[0] = 0;
        render_dipatchargs[0][0] = 0;
        render_dipatchargs[0][1] = 1;
        render_dipatchargs[0][2] = 1;
    }
}
