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
	model(std::string const& objpath, resourcelist& transientres, modelloadparams loadparams = {});
	model(std::vector<vertex> const& verts, std::vector<uint32> const& indices);

	template<typename geo_t>
	explicit model(geo_t&& geo) : model(geo.vertices(), geo.indices()) {}

	vertexattribs const& vertices() const { return _vertices; }
	std::vector<index> const& indices() const { return _indices; }
	std::vector<uint32> const& materials() const { return _materials; }
	std::vector<instance_data> instancedata() const;

	std::vector<index> _indices;
	vertexattribs _vertices;
	std::vector<uint32> _materials;
	std::vector<texture<accesstype::gpu>> _textures;
};

}
