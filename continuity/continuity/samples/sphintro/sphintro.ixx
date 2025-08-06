module;

#include "simplemath/simplemath.h"

export module sphintro;

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

export class sphfluid
{
	struct params
	{
		vector3 v;
		vector3 a;
		vector3 vp;
		vector3 p;
		float c;
		vector3 gc;
		float rho;
		float pr;
		bool surf;
		std::string matname;
	};

	geometry::aabb container;
	std::vector<params> particleparams;
	geometry::sphere particlegeometry;
public:
	sphfluid(geometry::aabb const& _bounds);

	float computetimestep() const;

	vector3 center();
	std::vector<uint8_t> texturedata() const;
	std::vector<gfx::vertex> vertices() const;
	std::vector<uint32> const& indices() const;
	std::vector<gfx::instance_data> instancedata() const;
	std::vector<gfx::vertex> particlevertices() const;
	void update(float dt);
	std::vector<gfx::vertex> fluidsurface;
	std::vector<uint32> fluidsurfaceindices;
};

export class sphfluidintro : public sample_base
{
public:
	sphfluidintro(view_data const& viewdata);

	gfx::resourcelist create_resources() override;
	void update(float dt) override;
	void render(float dt, gfx::renderer&) override;  

private:

	//std::vector<gfx::body_static<geometry::cube>> boxes;
	std::vector<gfx::body_dynamic<sphfluid>> fluid;
	//std::vector<gfx::body_static<sphfluid const&>> fluidparticles;
};


