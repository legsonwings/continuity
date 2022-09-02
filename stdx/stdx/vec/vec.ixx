module;
export module vec;

import <array>;
import <functional>;

import stdxcore;

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

	constexpr operator t() const requires (d == 1) { return (*this)[0]; }

	template<typename d_t>
	constexpr vec<d, d_t> castas() const { return { stdx::castas<d_t>(*this) }; }
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