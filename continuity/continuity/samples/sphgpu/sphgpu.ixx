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

export class sphgpu : public sample_base
{
public:
	sphgpu(view_data const& viewdata);

	gfx::resourcelist create_resources() override;
	void update(float dt) override;
	void render(float dt) override;  

private:

	float computetimestep() const;

	// replace with structured buffer
	gfx::rostructuredbuffer databuffer;

	gfx::proceduralblas procblas;
	gfx::tlas tlas;
	gfx::shadertable missshadertable;
	gfx::shadertable hitgroupshadertable;
	gfx::shadertable raygenshadertable;
	gfx::texture raytracingoutput;

	// todo : should be in engine
	gfx::structuredbuffer<uint32, gfx::accesstype::both> materialids;
	gfx::structuredbuffer<rt::material, gfx::accesstype::both> materials;

	ComPtr<ID3D12CommandSignature> render_commandsig;
	gfx::constantbuffer2<rt::sceneconstants, 1> constantbuffer;

	std::vector<gfx::body_static<geometry::cube>> boxes;

	bool debugmarch = false;
};


