module;

#include "shared/raytracecommon.h"

export module raytrace;

import engine;
import graphics;
import graphicscore;

export class raytrace : public sample_base
{
public:
	raytrace(view_data const& viewdata);

	gfx::resourcelist create_resources() override;
	void render(float dt, gfx::renderer&) override; 

private:

	gfx::triblas triblas;
	gfx::tlas tlas;
	gfx::shadertable missshadertable;
	gfx::shadertable hitgroupshadertable;
	gfx::shadertable raygenshadertable;
	gfx::rtouttexture raytracingoutput;
	gfx::rtvertexbuffer vertexbuffer;
	gfx::rtindexbuffer indexbuffer;
	gfx::structuredbuffer<uint32, gfx::accesstype::both> materialids;
	gfx::structuredbuffer<gfx::material, gfx::accesstype::both> materials;

	// todo : constantbuffer2 removed
	//gfx::constantbuffer2<rt::sceneconstants, 1> constantbuffer;
};
