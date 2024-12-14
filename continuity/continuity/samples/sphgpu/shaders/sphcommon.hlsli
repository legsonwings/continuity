#pragma once
#include "assets/common.hlsli"

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
    float3 marchingcubeoffset;
    float rho0; // reference density
    float viscosityconstant;
    float poly6coeff;
    float poly6gradcoeff;
    float spikycoeff;
    float viscositylapcoeff;
    float isolevel;
    float marchingcubesize;
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
                        UAV(u1), \
                        UAV(u2), \
                        UAV(u3), \
                        RootConstants(b1, num32bitconstants=23), \
                        "

ConstantBuffer<sphgpu_dispatch_params> sph_dispatch_params : register(b1);
RWStructuredBuffer<particle_data> particledata: register(u0);
RWStructuredBuffer<uint> isosurface_vertices_counter : register(u2);
RWStructuredBuffer<uint3> render_dipatchargs : register(u1);
RWStructuredBuffer<float3> isosurface_vertices : register(u3);