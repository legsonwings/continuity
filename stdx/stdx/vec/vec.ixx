export module vec;

import stdxcore;
import std;

export namespace stdx
{

template<uint d, stdx::arithmeticpure_c t = float>
struct vec : public std::array<t, d>
{
	static constexpr uint nd = d;
	constexpr vec operator-() const { return stdx::unaryop(*this, stdx::uminus()); }
	constexpr vec operator+(vec r) const { return stdx::binaryop(*this, r, std::plus<>()); }
	constexpr vec operator-(vec r) const { return stdx::binaryop(*this, r, std::minus<>()); }
	constexpr vec operator*(t r) const { return stdx::unaryop(*this, std::bind(std::multiplies<>(), std::placeholders::_1, r)); }
	constexpr vec operator/(t r) const { return stdx::unaryop(*this, std::bind(std::multiplies<>(), std::placeholders::_1, 1 / r)); }
	constexpr bool operator==(vec r) const { return stdx::equals(*this, r, t(0)); }
	constexpr bool operator==(t r) const { return stdx::equals(*this, r, t(0)); }

	constexpr operator t() const requires (d == 1) { return (*this)[0]; }

	constexpr t dot(vec const& r) const { return stdx::dot(*this, r); }
	constexpr vec normalized() const;
	constexpr vec cross(vec const& r) const requires (d == 3);
	constexpr t distancesqr(vec const& r) const;
	constexpr t distance(vec const& r) const;
	constexpr t length() const;

	static constexpr vec unit(uint ud) { vec v{ 0 }; v[ud] = t(1); return v; }
	static constexpr vec cross(vec const& l, vec const& r) requires (d == 3) { return l.cross(r); }
	static constexpr t distancesqr(vec const& l, vec const& r) { return l.distancesqr(r); }
	static constexpr t distance(vec const& l, vec const& r) { return l.distance(r); }

	template<typename d_t>
	constexpr vec<d, d_t> castas() const { return { stdx::castas<d_t>(*this) }; }

	constexpr std::string str() const;
};

template<stdx::arithmeticpure_c t, uint d>
vec<d, t> operator*(t l, vec<d, t> r) { return { stdx::unaryop(r, std::bind(std::multiplies<>(), std::placeholders::_1, l)) }; }

template<stdx::arithmeticpure_c t, uint d>
vec<d, t> operator+=(vec<d, t>& l, vec<d, t> r) { l = stdx::binaryop(l, r, std::plus<>()); return l; }

template<stdx::arithmeticpure_c t, uint d>
vec<d, t> operator-=(vec<d, t>& l, vec<d, t> r) { l = stdx::binaryop(l, r, std::minus<>()); return l; }

template<stdx::arithmeticpure_c t, uint d>
vec<d, t> operator*=(vec<d, t>& l, t r) { l = stdx::unaryop(l, std::bind(std::multiplies<>(), std::placeholders::_1, r)); return l; }

template<stdx::arithmeticpure_c t, uint d>
vec<d, t> operator/=(vec<d, t> l, t r) { l = stdx::unaryop(l, std::bind(std::multiplies<>(), std::placeholders::_1, t(1) / r)); return l; }

template<uint d>
using veci = vec<d, int>;

template<uint d>
using vecui = vec<d, uint>;

using vec1 = vec<1>;
using vec2 = vec<2>;
using vec3 = vec<3>;

using veci1 = veci<1>;
using veci2 = veci<2>;
using veci3 = veci<3>;

using vecui1 = vecui<1>;
using vecui2 = vecui<2>;
using vecui3 = vecui<3>;

template<uint d, stdx::arithmeticpure_c t>
constexpr vec<d, t> vec<d, t>::cross(vec<d, t> const& rhs) const requires (d == 3)
{
	vec<d, t> r;
	auto const& lhs = *this;
	r[0] = lhs[1] * rhs[2] - lhs[2] * rhs[1];
	r[1] = lhs[2] * rhs[0] - lhs[0] * rhs[2];
	r[2] = lhs[0] * rhs[1] - lhs[1] * rhs[0];
	
	return r;
}

template<uint d, stdx::arithmeticpure_c t>
constexpr t vec<d, t>::distancesqr(vec const& r) const
{
	auto const delta = r - *this;
	return delta.dot(delta);
}

template<uint d, stdx::arithmeticpure_c t>
constexpr t vec<d, t>::distance(vec const& r) const
{
	return std::sqrt(distancesqr(r));
}

template<uint d, stdx::arithmeticpure_c t>
constexpr t vec<d, t>::length() const
{
	return std::sqrt(this->dot(*this));
}

template<uint d, stdx::arithmeticpure_c t>
constexpr std::string vec<d, t>::str() const
{
	if constexpr (d == 0) return {};
	std::string r = std::to_string((*this)[0]);
	for (uint i(1); i < d; ++i)
		r += std::string(", ") + std::to_string((*this)[i]);
	return r;
}

template<uint d, stdx::arithmeticpure_c t>
constexpr vec<d, t> vec<d, t>::normalized() const
{
	vec r = *this;
	stdx::cassert([&]() { return !(r == 0); });
	return r / static_cast<t>(std::sqrt(r.dot(r)));
}

}

export namespace std
{
template <typename t, uint d>
class numeric_limits<stdx::vec<d, t>>
{
	static constexpr stdx::vec<d, t> min() noexcept { return t().fill(std::numeric_limits<stdx::containervalue_t<stdx::vec<d, t>>>::min()); }
	static constexpr stdx::vec<d, t> max() noexcept { return t().fill(std::numeric_limits<stdx::containervalue_t<stdx::vec<d, t>>>::max()); }
	static constexpr stdx::vec<d, t> lowest() noexcept { return t().fill(std::numeric_limits<stdx::containervalue_t<stdx::vec<d, t>>>::lowest()); }
};
}