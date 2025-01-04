#include "sphcommon.hlsli"

[RootSignature(ROOTSIG_SPHGPU)]
[NumThreads(64, 1, 1)]
void main(uint dtid : SV_DispatchThreadID)
{
    particledata[dtid].a = (float3) 0.0f;
    for (uint i = 0; i < sph_dispatch_params.numparticles; ++i)
    {
        float3 pton = particledata[i].p - particledata[dtid].p;
        float distsqr = dot(pton, pton);
        
        if (distsqr < sph_dispatch_params.hsqr)
        {
            float diff = sph_dispatch_params.h - sqrt(distsqr);

            // get to neighbour direction safely(dir is zero if vector is zero)
            pton = pton / (FLOAT_EPSILON + sqrt(distsqr));
            particledata[dtid].a += pton * (sph_dispatch_params.spikycoeff * (particledata[dtid].pr + particledata[i].pr) * diff * diff / (2.0f * particledata[dtid].rho * particledata[i].rho));
            particledata[dtid].a += sph_dispatch_params.viscosityconstant * (particledata[i].v - particledata[dtid].v) * sph_dispatch_params.viscositylapcoeff * diff / particledata[i].rho;
        }
    }

    float accsqr = dot(particledata[dtid].a, particledata[dtid].a);
    
    // todo : send maxacca as param
    if (accsqr > 100.0f * 100.0f)
    {
        particledata[dtid].a = (particledata[dtid].a / sqrt(accsqr)) * 100.0f;
    }
    
    float vsqr = dot(particledata[dtid].v, particledata[dtid].v);

    // todo : send maxv as param
    if (vsqr > 20.0f * 20.0f)
    {
        particledata[dtid].v = (particledata[dtid].v / sqrt(vsqr)) * 20.0f;
    }
    
    // todo : should be a constant param
    particledata[dtid].a += float3(0.0f, -2.0f, 0.0f);
 
    // reflect velocity
    float3 localpt = particledata[dtid].p - sph_dispatch_params.containerorigin;
    float3 halfextents = sph_dispatch_params.containerextents / 2.0f;
    float3 localpt_abs = abs(localpt);
    
    if (any(localpt_abs > halfextents))
    {
        float3 penetration = localpt_abs - halfextents;
        float3 normal = -normalize(max(penetration, 0.0f) * normalize(localpt));

        float3 impulsealongnormal = dot(particledata[dtid].v, -normal);
        
        float const restitution = 0.3f;
        particledata[dtid].p += penetration * normal;
        particledata[dtid].v += (1.0f + restitution) * impulsealongnormal * normal;
    }
    
    // todo : clamp acceleration and velocity??
    particledata[dtid].vp = particledata[dtid].v + particledata[dtid].a * sph_dispatch_params.dt;
    particledata[dtid].p += particledata[dtid].vp * sph_dispatch_params.dt;
    particledata[dtid].v = particledata[dtid].vp + particledata[dtid].a * (0.5f * sph_dispatch_params.dt);
}
