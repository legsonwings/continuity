#pragma once
#include "assets/common.hlsli"

#define FLT_EPSILON 1e-9f

// globals
// various dispatch params(numprims, etc)
// particle data buffer

struct sphgpu_dispatch_params
{
    uint numparticles;
    float dt;
    float particleradius;
    float h; // smoothing kernel constant
    float3 containerorigin;
    float hsqr;
    float3 containerextents;
    float k;  // pressure constant
    float rho0; // reference density
    float viscosityconstant;
    float poly6coeff;
    float poly6gradcoeff;
    float spikycoeff;
    float viscositylapcoeff;
};

struct particle_data
{
    float3 v;
    float3 vp;
    float3 p;
    float3 a;
    float3 gc;
    float c;
    float rho;
    float pr;
};

#define ROOTSIG_SPHGPU "CBV(b0), \
                        UAV(u0), \
                        RootConstants(b1, num32bitconstants=20), \
                        "

ConstantBuffer<sphgpu_dispatch_params> sph_dispatch_params : register(b1);
RWStructuredBuffer<particle_data> particledata: register(u0);