module;

#include "simplemath/simplemath.h"

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

	gfx::staticgpubuffer databuffer;

	std::vector<gfx::body_static<geometry::cube>> boxes;
};


