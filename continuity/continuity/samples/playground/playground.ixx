module;

#include "simplemath/simplemath.h"
#include "shared/common.h"

export module playground;

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

template<dbody_c body_t, gfx::topology primitive_t = gfx::topology::triangle>
class body_dynamic;

}

using vector3 = DirectX::SimpleMath::Vector3;
using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;

export class playground : public sample_base
{
public:
	playground(view_data const& viewdata);

	gfx::resourcelist create_resources() override;
	void update(float dt) override;
	void render(float dt) override;  

private:

	std::vector<gfx::body_static<gfx::model>> models;
	std::vector<gfx::body_static<geometry::cube>> boxes;

	gfx::structuredbuffer<uint32, gfx::accesstype::both> materialids;
	gfx::structuredbuffer<gfx::material, gfx::accesstype::both> materials;
};


