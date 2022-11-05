module;

#define __SPECSTRINGS_STRICT_LEVEL 0
#include "simplemath/simplemath.h"

export module cursor;

import stdxcore;
import geocore;
import vec;

import <array>;
import <optional>;
import <functional>;

using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;

using vec2 = stdx::vec2;
using vec3 = stdx::vec3;
using veci2 = stdx::veci2;
using line = geometry::line;

export namespace continuity
{

class cursor
{
public:
	cursor() : _pos(pos()), _lastpos(pos()) { };

	void tick(float dt);
	vec2 pos() const;
	vec2 vel() const;
	veci2 posicentered() const;

	line ray(float nearp, float farp, matrix const& view, matrix const& proj) const;
	vec3 to3d(vec3 pos, float nearp, float farp, matrix const& view, matrix const& proj) const;

private:
	vec2 _lastpos = {};
	vec2 _pos = {};
	vec2 _vel = {};
};

}