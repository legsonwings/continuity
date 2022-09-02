module;
export module stdxcore;

import <array>;
import <vector>;
import <limits>;
import <ranges>;
import <concepts>;
import <concepts>;
import <type_traits>;

export using uint = std::size_t;

export namespace stdx
{

std::ranges::iota_view<uint, uint> range(uint e) { return std::ranges::iota_view<uint, uint>(0u, e); }
std::ranges::iota_view<uint, uint> range(uint s, uint e) { return std::ranges::iota_view<uint, uint>(s, e); }

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
	{v* v}-> std::convertible_to<t>;
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

template <typename t = float> 
requires arithmetic_c<t>
t constexpr tolerance = t(1e-4f);

template <typename t> requires arithmetic_c<t>
struct invalid { constexpr operator t() const { return { std::numeric_limits<t>::max() }; } };

struct uminus { constexpr auto operator() (arithmeticpure_c auto v) const { return -v; }; };

bool constexpr nearlyequal(arithmeticpure_c auto const& l, arithmeticpure_c auto const& r) { return l == r; }

template <typename t>
requires arithmetic_c<t>
bool constexpr isvalid(t const& val) { return !nearlyequal(std::numeric_limits<t>::max(), val); }

template<typename t>
using containervalue_t = std::decay_t<t>::value_type;

template <uint alignment, typename t>
constexpr bool isaligned(t value) { return ((uint)value & (alignment - 1)) == 0; }

constexpr bool ispowtwo(uint value);
constexpr uint nextpowoftwomultiple(uint value, uint multipleof);
constexpr int ceil(float value);
constexpr auto pown(arithmeticpure_c auto v, uint n)
{
	decltype(v) res = 1;
	for (auto i(0); i < n; ++i)
		res *= v;
	return res;
}

template<indexablecontainer_c t>
void ensuresize(t& c, uint size) {}

template<uint n>
requires (n > 0)
constexpr std::array<uint, n> getdigits(uint d)
{
	std::array<uint, n> ret{};

	uint i = d;
	uint j = n - 1;
	for (; i > 9; i /= 10, --j)
		ret[j] = i % 10;
	ret[j] = i;
	return ret;
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
requires stdx::arithmeticpure_c<containervalue_t<l_t>>&& stdx::arithmeticpure_c<containervalue_t<r_t>>
constexpr auto dot(l_t&& a, r_t&& b)
{
	using v_t = containervalue_t<l_t>;
	v_t r = v_t(0);
	for (uint i(0); i < std::min(a.size(), b.size()); ++i) r += a[i] * b[i];
	return r;
}

template<indexablecontainer_c t>
requires stdx::arithmeticpure_c<containervalue_t<t>>
constexpr auto clamp(t&& a, containervalue_t<t> l, containervalue_t<t> h)
{
	std::decay_t<t> r;
	ensuresize(r, a.size());
	for (uint i(0); i < a.size(); ++i) r[i] = (a[i] < l ? l : (a[i] > h ? h : a[i]));
	return r;
}

template<indexablecontainer_c t>
requires stdx::arithmeticpure_c<containervalue_t<t>>
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
template<typename t, uint n, uint d = 0>
requires (n >= 0 && arithmetic_c<t>)
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

template<typename t, uint n, uint d>
requires (n >= 0 && d == n + 1 && arithmetic_c<t>)
struct nlerp<t, n, d>
{
	static constexpr t lerp(std::array<t, 1> const& data, std::array<float, n + 1> alpha) { return data[0]; }
};

template <arithmetic_c t, uint n, uint d = 0>
constexpr auto lerp(std::array<t, stdx::pown(2, n - d + 1)> const& data, std::array<float, n + 1> alpha) { return nlerp<t, n, d>::lerp(data, alpha); }

}