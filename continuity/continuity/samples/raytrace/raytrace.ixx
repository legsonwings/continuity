module;

#include "simplemath/simplemath.h"
#include "thirdparty/d3dx12.h"

export module raytrace;

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

export class raytrace : public sample_base
{
public:
	raytrace(view_data const& viewdata);

	gfx::resourcelist create_resources() override;
	void update(float dt) override;
	void render(float dt) override;  

private:

	gfx::triblas triblas;
	gfx::proceduralblas procblas;
	gfx::tlas tlas;
	gfx::shadertable missshadertable;
	gfx::shadertable hitgroupshadertable;
	gfx::shadertable raygenshadertable;
	ComPtr<ID3D12Resource> raytracingoutput;
	D3D12_GPU_DESCRIPTOR_HANDLE raytracingoutput_uavgpudescriptor;

	ComPtr<ID3D12CommandSignature> render_commandsig;

	std::vector<gfx::body_static<geometry::cube>> boxes;
};


