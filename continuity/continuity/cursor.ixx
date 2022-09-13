module;

#define __SPECSTRINGS_STRICT_LEVEL 0
#include "simplemath/simplemath.h"

export module cursor;

import stdxcore;
import vec;

import <array>;
import <functional>;

using vector2 = DirectX::SimpleMath::Vector2;
using vector3 = DirectX::SimpleMath::Vector3;
using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;
using plane = DirectX::SimpleMath::Plane;

export namespace continuity
{

class cursor
{
public:
	cursor() : _pos(pos()), _lastpos(pos()) { };

	void tick(float dt);
	vector2 pos() const;
	vector2 vel() const;
	stdx::veci2 posicentered() const;
	vector3 ray(float nearp, float farp, matrix const& view, matrix const& proj) const;
	vector3 to3d(vector3 pos, float nearp, float farp, matrix const& view, matrix const& proj) const;

private:
	vector2 _lastpos = {};
	vector2 _pos = {};
	vector2 _vel = {};
};

}