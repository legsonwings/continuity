module;

#include "simplemath/simplemath.h"
#include "thirdparty/d3dx12.h"
#include "shared/raytracecommon.h"

export module sphgpu;

import activesample;
import engine;
import graphics;
import geometry;
import body;

import std;

namespace gfx
{

struct instance_data;

template<sbody_c body_t, gfx::topology primitive_t = gfx::topology::triangle>
class body_static;

}

using vector3 = DirectX::SimpleMath::Vector3;
using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;

// todo : move these to shared
struct alignas(256) sphconstants
{
	uint32 numparticles;
	float dt;
	float particleradius;
	float h;
	
	stdx::vec3 containerextents;
	float hsqr;

	float k;
	float rho0;
	float viscosityconstant;
	float poly6coeff;
	float poly6gradcoeff;
	float spikycoeff;
	float viscositylapcoeff;
	float isolevel;
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

export class sphgpu : public sample_base
{
public:
	sphgpu(view_data const& viewdata);

	gfx::resourcelist create_resources() override;
	void update(float dt) override;
	void render(float dt) override;  

private:

	float computetimestep() const;

	gfx::structuredbuffer<particle_data, gfx::accesstype::gpu> databuffer;

	gfx::triblas triblas;
	gfx::proceduralblas procblas;
	gfx::tlas tlas;
	gfx::shadertable missshadertable;
	gfx::shadertable hitgroupshadertable;
	gfx::shadertable raygenshadertable;
	gfx::rtouttexture raytracingoutput;

	// todo : should be in engine
	gfx::structuredbuffer<uint32, gfx::accesstype::both> materialids;
	gfx::structuredbuffer<gfx::material, gfx::accesstype::both> materials;

	//gfx::constantbuffer2<rt::sceneconstants, 1> constantbuffer;
	//gfx::constantbuffer2<sphconstants, 1> sphconstants;

	gfx::rtvertexbuffer roomvertbuffer;
	gfx::rtindexbuffer roomindexbuffer;
};


