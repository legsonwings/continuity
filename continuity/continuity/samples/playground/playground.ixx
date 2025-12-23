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
class renderer;
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

	gfx::resourcelist create_resources(gfx::renderer& renderer) override;
	void update(float dt) override;
	void render(float dt, gfx::renderer& renderer) override;
	void on_key_up(unsigned key) override;

private:

	struct rootdescriptors
	{
		uint32 dispatchparams;
		uint32 viewglobalsdesc;
		uint32 sceneglobalsdesc;
	} rootdescs;

	struct viewglobals
	{
		stdx::vec3 viewpos;
		stdx::matrix4x4 viewproj;
	};

	struct sceneglobals
	{
		uint32 matbuffer;
		uint32 shadowmap;
		uint32 viewdirshading;
		stdx::vec3 lightdir;
		float lightluminance;
	};

	std::vector<gfx::body_static<gfx::model>> models;

	gfx::structuredbuffer<viewglobals, gfx::accesstype::both> viewglobalsbuffer;
	gfx::structuredbuffer<sceneglobals, gfx::accesstype::both> sceneglobalsbuffer;

	uint32 viewdirshading = 0;
};


