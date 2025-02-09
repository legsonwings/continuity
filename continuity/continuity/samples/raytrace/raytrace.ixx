module;

#include "raytracecommon.h"

export module raytrace;

import engine;
import graphics;
import graphicscore;

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
	gfx::texture raytracingoutput;

	gfx::constantbuffer2<rt::sceneconstants, frame_count> constantbuffer;
};


