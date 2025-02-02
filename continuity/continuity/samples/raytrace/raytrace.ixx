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

export class raytrace : public sample_base
{
public:
	raytrace(view_data const& viewdata);

	gfx::resourcelist create_resources() override;
	void render(float dt) override;  

private:

	gfx::triblas triblas;
	gfx::tlas tlas;
	gfx::shadertable missshadertable;
	gfx::shadertable hitgroupshadertable;
	gfx::shadertable raygenshadertable;
	gfx::uav raytraceoutput_uav;
	gfx::texture raytracingoutput;
};


