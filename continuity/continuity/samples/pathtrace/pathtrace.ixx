module;

#include "simplemath/simplemath.h"
#include "shared/raytracecommon.h"

export module pathtrace;

import stdx;
import engine;
import graphics;
import graphicscore;

using matrix = DirectX::SimpleMath::Matrix;

export class pathtrace : public sample_base
{
public:
	pathtrace(view_data const& viewdata);

	gfx::resourcelist create_resources(gfx::renderer& renderer) override;
	void render(float dt, gfx::renderer& renderer) override;
	void on_key_up(unsigned key) override;

private:

	rt::rootdescs rootdescs;

	gfx::model model;

	stdx::ext<gfx::structuredbuffer<rt::ptsettings, gfx::accesstype::both>, uint32> ptsettingsbuffer;
	gfx::structuredbuffer<rt::dispatchparams, gfx::accesstype::both> dispatchparams;
	gfx::structuredbuffer<stdx::vec3, gfx::accesstype::both> posbuffer;
	gfx::structuredbuffer<stdx::vec2, gfx::accesstype::both> texcoordbuffer;
	gfx::structuredbuffer<gfx::tbn, gfx::accesstype::both> tbnbuffer;
	gfx::structuredbuffer<gfx::index, gfx::accesstype::both> indexbuffer;
	gfx::structuredbuffer<uint32, gfx::accesstype::both> materialsbuffer;
	stdx::ext<gfx::structuredbuffer<rt::viewglobals, gfx::accesstype::both>, uint32> viewglobalsbuffer;
	stdx::ext<gfx::structuredbuffer<rt::sceneglobals, gfx::accesstype::both>, uint32> sceneglobalsbuffer;

	gfx::triblas triblas;
	gfx::tlas tlas;
	gfx::shadertable raygenshadertable;
	gfx::rtouttexture raytracingoutput;

	uint32 framecount = 0;
	rt::sceneglobals scenedata;
	rt::ptsettings ptsettings = rt::ptmodesettings[rt::ptmode];

	matrix prevviewmatrix;

	bool accumdirty = false;
};
