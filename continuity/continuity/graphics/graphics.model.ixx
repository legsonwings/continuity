module;

#include "simplemath/simplemath.h"

export module graphics:model;

import stdxcore;
import stdx;
import std;
import :resourcetypes;

import graphicscore;

export namespace gfx
{

struct modelloadparams
{
	bool specularasmetallicroughness = true;
	bool ambientasnormal = true;
	bool translatetoorigin = false;
};

struct model
{
	model() = default;
	model(std::string const& objpath, modelloadparams loadparams = {});

	std::vector<vertex> const& vertices() const { return _vertices; }
	std::vector<uint32> const& indices() const { return _indices; }

	// center at origin for now
	std::vector<instance_data> instancedata() const;

	std::vector<uint32> _indices;
	std::vector<vertex> _vertices;
	std::vector<texture<accesstype::gpu>> _textures;

	uint32 _primmaterialsdescidx;
	structuredbuffer<uint32, gfx::accesstype::both> _primitivematerials;
};

}
