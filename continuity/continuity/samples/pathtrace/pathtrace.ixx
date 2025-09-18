module;

#include "simplemath/simplemath.h"
#include "shared/raytracecommon.h"

export module pathtrace;

import engine;
import graphics;
import graphicscore;

using matrix = DirectX::SimpleMath::Matrix;

export class pathtrace : public sample_base
{
public:
	pathtrace(view_data const& viewdata);

	gfx::resourcelist create_resources(gfx::deviceresources& deviceres) override;
	void update(float dt) override;
	void render(float dt, gfx::renderer& renderer) override;
	void on_key_up(unsigned key) override;

private:

	rt::rootdescs rootdescs;

	gfx::model model;

	gfx::structuredbuffer<rt::dispatchparams, gfx::accesstype::both> dispatchparams;
	gfx::structuredbuffer<stdx::vec3, gfx::accesstype::both> posbuffer;
	gfx::structuredbuffer<stdx::vec2, gfx::accesstype::both> texcoordbuffer;
	gfx::structuredbuffer<gfx::tbn, gfx::accesstype::both> tbnbuffer;
	gfx::structuredbuffer<gfx::index, gfx::accesstype::both> indexbuffer;
	gfx::structuredbuffer<uint32, gfx::accesstype::both> materialsbuffer;
	gfx::structuredbuffer<rt::viewglobals, gfx::accesstype::both> viewglobalsbuffer;
	gfx::structuredbuffer<rt::sceneglobals, gfx::accesstype::both> sceneglobalsbuffer;

	gfx::triblas triblas;
	gfx::tlas tlas;
	gfx::shadertable missshadertable;
	gfx::shadertable hitgroupshadertable;
	gfx::shadertable raygenshadertable;
	gfx::rtouttexture raytracingoutput;

	uint32 framecount = 0;
	uint32 rtoutputuavidx = stdx::invalid<uint32>;
	rt::sceneglobals scenedata;

	matrix prevviewmatrix;
};
