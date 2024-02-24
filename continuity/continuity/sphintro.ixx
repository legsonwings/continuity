
export module sphintro;

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

template<dbody_c body_t, gfx::topology primitive_t = gfx::topology::triangle>
class body_dynamic;

}

export class sphfluidintro : public sample_base
{
public:
	sphfluidintro(view_data const& viewdata);

	gfx::resourcelist load_assets_and_geometry() override;
	void update(float dt) override;
	void render(float dt) override;

private:
	std::vector<gfx::body_static<geometry::cube>> boxes;
	std::vector<gfx::body_static<geometry::sphere>> particles;
};


