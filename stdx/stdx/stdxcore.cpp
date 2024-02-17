module stdxcore;

import <cassert>;

namespace stdx
{

bool ispowtwo(uint value) { return (value & (value - 1)) == 0; }

uint nextpowoftwomultiple(uint value, uint multipleof)
{
	assert(((multipleof & (multipleof - 1)) == 0));
	auto const mask = multipleof - 1;
	return (value + mask) & ~mask;
}

int ceil(float value)
{
	int intval = static_cast<int>(value);
	if (value == static_cast<float>(intval))
		return intval;
	return intval + (value > 0.f ? 1 : 0);
}

}