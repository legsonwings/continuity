module;

#include "simplemath/simplemath.h"
#include "thirdparty/d3dx12.h"

export module sphgpu;

import activesample;
import engine;
import graphics;
import geometry;
import body;

import std.core;

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

	gfx::structuredbuffer databuffer;
	gfx::structuredbuffer isosurface_vertices_counter;
	gfx::structuredbuffer isosurface_vertices;
	gfx::structuredbuffer render_args;

    // ray trace stuff
	gfx::triblas triblas;
	gfx::proceduralblas procblas;
	gfx::tlas tlas;
    ComPtr<ID3D12Resource> missShaderTable;
    ComPtr<ID3D12Resource> hitGroupShaderTable;
    ComPtr<ID3D12Resource> rayGenShaderTable;
	ComPtr<ID3D12Resource> raytracingOutput;
	D3D12_GPU_DESCRIPTOR_HANDLE raytracingOutputResourceUAVGpuDescriptor;

	ComPtr<ID3D12CommandSignature> render_commandsig;

	std::vector<gfx::body_static<geometry::cube>> boxes;

	bool debugmarch = false;
};


