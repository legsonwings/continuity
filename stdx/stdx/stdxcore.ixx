module;

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#endif

#include <cassert>

export module stdxcore;

import std;

// unexported declarations
namespace stdx
{

void cassertinternal(bool const passed, std::source_location loc, std::string msg = "");

}

export using uint8 = unsigned char;
export using uint16 = unsigned short;
export using uint32 = unsigned int;
export using uint = std::size_t;

export namespace stdx
{

struct nulltype { };

template<typename t> constexpr bool is_nulltype = std::is_same_v<t, nulltype>;

template <typename t> concept uint_c = std::convertible_to<t, uint>;

template <typename t>
concept lvaluereference_c = std::is_lvalue_reference_v<t>;

template <typename t>
concept rvaluereference_c = std::is_rvalue_reference_v<t>;

template <typename t>
concept triviallycopyable_c = std::is_trivially_copyable_v<t>;

template <typename t>
concept arithmeticpure_c = std::is_arithmetic_v<t>;

template <typename t>
concept arithmetic_c = requires(t v)
{
	{-v}-> std::convertible_to<t>;
	{v * v}-> std::convertible_to<t>;
	{v + v} -> std::convertible_to<t>;
	{v - v} -> std::convertible_to<t>;
	{v / v} -> std::convertible_to<t>;
};

template <typename t>
concept indexablecontainer_c = requires(t v)
{
	{v.size()} -> std::convertible_to<std::size_t>;
	v.operator[](std::size_t());
};

template <typename t, typename u>
concept samedecay_c = std::same_as<std::decay_t<t>, std::decay_t<u>>;

template <arithmetic_c t = float>
t constexpr tolerance = t{ 1e-5f };

template <arithmetic_c t>
t constexpr invalid = std::numeric_limits<t>::max();

template<arithmeticpure_c t>
std::ranges::iota_view<t, t> range(t e) { return std::ranges::iota_view(t(0), e); }

template<arithmeticpure_c t>
std::ranges::iota_view<t, t> range(t s, t e) { return std::ranges::iota_view(s, e); }

struct uminus { constexpr auto operator() (arithmeticpure_c auto v) const { return -v; }; };

constexpr auto sign(arithmeticpure_c auto const& v) { return std::decay_t<decltype(v)>((v > 0) - (v < 0)); }

constexpr auto sign(indexablecontainer_c auto const& v)
{
	std::decay_t<decltype(v)> r;
	ensuresize(r, v.size());
	for (auto i(0); i < r.size(); ++i)
	{
		r[i] = sign(v[i]);
	}

	return r;
}

constexpr auto abs(indexablecontainer_c auto const& v)
{
	std::decay_t<decltype(v)> r;
	ensuresize(r, v.size());
	for (auto i(0); i < r.size(); ++i)
	{
		r[i] = std::abs(v[i]);
	}

	return r;
}

template<arithmeticpure_c t = float>
constexpr bool equals(t const& l, t const& r, t tol = tolerance<t>) { return std::abs(l - r) <= tol; }

template <arithmetic_c t>
constexpr bool isvalid(t const& val) { return !equals(std::numeric_limits<t>::max(), val); }

template<typename t>
using containervalue_t = std::decay_t<t>::value_type;

template <uint alignment, typename t>
constexpr bool isaligned(t value) { return ((uint)value & (alignment - 1)) == 0; }

template<std::predicate t>
void cassert(t const& expr, std::string msg = "", std::source_location loc = std::source_location::current())
{
#ifndef NDEBUG
	cassertinternal(expr(), loc, msg);
#endif
}

void cassert(bool const passed, std::string msg = "", std::source_location loc = std::source_location::current())
{
#ifndef NDEBUG
	cassertinternal(passed, loc, msg);
#endif
}

constexpr bool ispowtwo(uint value);
uint nextpowoftwomultiple(uint value, uint multipleof);
constexpr int ceil(float value);

template<indexablecontainer_c t>
void ensuresize(t& c, uint size) {}

template<typename t>
void ensuresize(std::vector<t>& v, uint size) { v.resize(size); }

constexpr auto pown(arithmeticpure_c auto const v, uint n)
{
	std::decay_t<decltype(v)> res = 1;
	for (auto i(0); i < n; ++i)
		res *= v;
	return res;
}

template<arithmeticpure_c t = float>
constexpr bool equals(indexablecontainer_c auto const& a, t b, t tol = tolerance<t>)
{
	for (auto const& e : a)
		if (!equals(e, b, tol))
			return false;

	return true;
}

template<arithmeticpure_c t = float>
constexpr bool equals(indexablecontainer_c auto const& a, indexablecontainer_c auto const& b, t tol = tolerance<t>)
{
	for (uint i(0); i < std::min(a.size(), b.size()); ++i)
		if (!equals(a[i], b[i], tol))
			return false;

	return true;
}

template<typename t>
void append(std::vector<t>&& source, std::vector<t>& dest)
{
	dest.reserve(dest.size() + source.size());
	std::move(source.begin(), source.end(), std::back_inserter(dest));
}

template<typename t>
void append(std::vector<t> const& source, std::vector<t>& dest)
{
	dest.reserve(dest.size() + source.size());
	for (auto const& e : source) { dest.emplace_back(e); }
}

template<typename d_t, typename s_t, uint n>
constexpr auto castas(std::array<s_t, n> a)
{
	std::array<d_t, n> r;
	for (uint i(0); i < n; ++i) r[i] = static_cast<d_t>(a[i]);
	return r;
}

template<indexablecontainer_c l_t, indexablecontainer_c r_t>
requires arithmeticpure_c<containervalue_t<l_t>> && stdx::arithmeticpure_c<containervalue_t<r_t>>
constexpr auto dot(l_t&& a, r_t&& b)
{
	using v_t = containervalue_t<l_t>;
	v_t r = v_t(0);
	for (uint i(0); i < std::min(a.size(), b.size()); ++i) r += a[i] * b[i];
	return r;
}

template<indexablecontainer_c t>
constexpr auto clamp(indexablecontainer_c auto const& v, indexablecontainer_c auto const& l, indexablecontainer_c auto const& h)
{
	std::decay_t<t> r;
	ensuresize(r, v.size());
	for (uint i(0); i < v.size(); ++i) r[i] = (v[i] < l[i] ? l[i] : (v[i] > h[i] ? h[i] : v[i]));
	return r;
}

template<indexablecontainer_c t>
requires arithmeticpure_c<containervalue_t<t>>
constexpr auto clamp(t&& a, containervalue_t<t> l, containervalue_t<t> h)
{
	std::decay_t<t> r;
	ensuresize(r, a.size());
	for (uint i(0); i < a.size(); ++i) r[i] = (a[i] < l ? l : (a[i] > h ? h : a[i]));
	return r;
}

template<indexablecontainer_c t>
requires arithmeticpure_c<containervalue_t<t>>
constexpr void clamp(t& a, containervalue_t<t> l, containervalue_t<t> h)
{
	for (uint i(0); i < a.size(); ++i) a[i] = (a[i] < l ? l : (a[i] > h ? h : a[i]));
}

template<indexablecontainer_c t, typename f_t>
requires std::invocable<f_t, containervalue_t<t>>
constexpr auto unaryop(t&& a, f_t f)
{
	std::decay_t<t> r;
	ensuresize(r, a.size());
	for (uint i(0); i < a.size(); ++i) r[i] = f(a[i]);
	return r;
}

template<indexablecontainer_c l_t, indexablecontainer_c r_t, typename f_t>
requires std::invocable<f_t, containervalue_t<l_t>, containervalue_t<r_t>>
constexpr auto binaryop(l_t&& a, r_t&& b, f_t f)
{
	std::decay_t<l_t> r;
	ensuresize(r, std::min(a.size(), b.size()));
	for (uint i(0); i < std::min(a.size(), b.size()); ++i) r[i] = f(a[i], b[i]);
	return r;
}

// type, lerp degree, current dimension being lerped
template<arithmetic_c t, uint n, uint d = 0>
requires (n >= 0)
struct nlerp
{
	static constexpr t lerp(std::array<t, stdx::pown(2, n - d + 1)> const& data, std::array<float, n + 1> alpha)
	{
		std::array<t, stdx::pown(2, n - d)> r;
		for (uint i(0); i < r.size(); ++i)
			r[i] = data[i * 2] * (1.f - alpha[d]) + data[i * 2 + 1] * alpha[d];
		return nlerp<t, n, d + 1>::lerp(r, alpha);
	}
};

template<arithmetic_c t, uint n, uint d>
requires (n >= 0 && d == n + 1)
struct nlerp<t, n, d>
{
	static constexpr t lerp(std::array<t, 1> const& data, std::array<float, n + 1> alpha) { return data[0]; }
};

template <arithmetic_c t, uint n, uint d = 0>
constexpr auto lerp(std::array<t, stdx::pown(2, n - d + 1)> const& data, std::array<float, n + 1> alpha) { return nlerp<t, n, d>::lerp(data, alpha); }

}

namespace stdx
{

void cassertinternal(bool const passed, std::source_location loc, std::string msg)
{
	if (!passed)
	{
#ifdef _WIN32
		msg = std::string("\nAssertion failed in file: ") + loc.file_name() + "(" + std::to_string(loc.line()) + ":" + std::to_string(loc.column()) + ")" + ", function : " + loc.function_name() + "\nMsg : " + msg;
		OutputDebugStringA(msg.c_str());
		OutputDebugStringA("\n\n");
		if (IsDebuggerPresent())
		{
			__debugbreak();
		}
#else
#error "not implemented for this platform";
#endif
	}
}

}
