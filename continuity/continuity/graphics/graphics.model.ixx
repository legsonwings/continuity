module;

#include "simplemath/simplemath.h"

export module graphics:model;

import stdxcore;
import stdx;
import std;

import graphicscore;

export namespace gfx
{

export struct model
{
	model() = default;
	model(std::string const& objpath, bool translatetoorigin = false);

	std::vector<vertex> const& vertices() const { return _vertices; }
	std::vector<uint32> const& indices() const { return _indices; }

	// center at origin for now
	std::vector<instance_data> instancedata() const;

	std::vector<uint32> _indices;
	std::vector<vertex> _vertices;
};

}
