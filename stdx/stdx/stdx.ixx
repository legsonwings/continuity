module;

export module stdx;

import <cmath>;
import <cstddef>;
import <cassert>;
import <array>;
import <vector>;
import <limits>;
import <ranges>;
import <concepts>;
import <algorithm>;
import <concepts>;
import <type_traits>;

import stdxcore;
import vec;

export namespace stdx
{

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

// triangular index
template<uint n>
struct triindex
{
	constexpr triindex(uint _i, uint _j, uint _k) : i(_i), j(_j), k(_k) {}
	constexpr uint to1d() const { return to1d(this->j, this->k); }
	static constexpr uint to1d(uint j, uint k) { return (n - j) * (n - j + 1) / 2 + k; }         // n - j gives us row index

	static triindex to3d(uint idx1d)
	{
		// determine which row the control point belongs to
		// this was derived empirically
		float const temp = (std::sqrt((static_cast<float>(idx1d) + 1.f) * 8.f + 1.f) - 1.f) / 2.0f - tolerance<>;

		// convert to 0 based index
		uint const row = static_cast<uint>(ceil(temp)) - 1u;

		// this will give the index of starting vertex of the row
		// number of vertices till row n(0-based) is n(n+1)/2
		uint const rowstartvertex = (row) * (row + 1) / 2;

		uint const j = n - row;
		uint const k = idx1d - rowstartvertex;
		uint const i = n - j - k;

		return triindex(i, j, k);
	}

	uint i, j, k;
};

// n is dimension of the grid(0 based)
template<uint n>
requires (n >= 0)
struct grididx : public vec<n + 1, uint>
{
	constexpr grididx() = default;
	constexpr grididx(vec<n + 1, uint> const & v) : stdx::vecui<n + 1>(v) {}
	explicit constexpr grididx(uint idx) : stdx::vecui<n + 1>(getdigits<n + 1>(idx)) {}
	template<uint_c ... args>
	requires (sizeof...(args) == (n + 1))
	constexpr grididx(args... _coords) : grididx{ static_cast<uint>(_coords)... } {}

	// d is degree(0 based)
	static constexpr uint to1d(uint d, grididx const& idx)
	{
		uint res = 0;
		// idx = x0 + x1d + x2dd + x3ddd + ....
		for (uint i(n); i >= 1; --i)
			res = (idx[i] + res) * (d + 1);

		res += idx[0];
		return res;
	}

	static constexpr grididx from1d(uint d, uint idx)
	{
		grididx res;
		for (auto i(0); i < n + 1; ++i)
			res[i] = (idx / pown((d + 1), i)) % (d + 1);
		return res;
	}

	static constexpr grididx unit(uint d)
	{
		grididx res;
		res[d] = 1;
		return res;
	}
};

template<typename b, typename e>
struct ext
{
	b base;
	e ext;

	constexpr ext(b const& _b, e const& _e) : base(_b), ext(_e) {}

	constexpr operator b& () { return base; }
	constexpr operator b const& () const { return base; }
	constexpr e& ex() { return ext; }
	constexpr e const& ex() const { return ext; }
	constexpr b& get() { return base; }
	constexpr b const& get() const { return base; }
	constexpr b& operator*() { return base; }
	constexpr b const& operator*() const { return base; }
	constexpr b* operator->() { return &base; }
	constexpr b const* operator->() const { return &base; }
};

// these allow iteration over multiple heterogenous containers
template<typename t, indexablecontainer_c... args_t>
struct join;

template<typename t, indexablecontainer_c... args_t>
struct joiniter
{
	using idx_t = uint;
	using join_t = join<t, args_t...>;

	joiniter(join_t* _join, uint _idx) : join(_join), idx(_idx) {}
	t* operator*() { return join->get(idx); }
	bool operator!=(joiniter const& rhs) const { return idx != rhs.idx || join != rhs.join; }
	joiniter& operator++() { idx++; return *this; }

	idx_t idx;
	join_t* join;
};

template<typename t, indexablecontainer_c... args_t>
struct join
{
private:
	using iter_t = joiniter<t, args_t...>;
	friend struct iter_t;
	static constexpr uint mysize = sizeof...(args_t);

public:
	join(args_t&... _args) : data(_args...) {}
	iter_t begin()
	{
		if (calcsizes() == 0) return end();
		return iter_t(this, 0);
	}
	iter_t end() { return iter_t(this, calcsizes()); }

private:
	uint calcsizes()
	{
		uint totalsize = 0;
		for (uint i = 0; i < mysize; ++i) { sizes[i] = getsize(i); totalsize += sizes[i]; }
		return totalsize;
	}

	std::pair<uint, uint> container(uint idx) const
	{
		static constexpr auto invalidcont = std::make_pair(invalid<uint>(), invalid<uint>());
		uint sum = 0;
		std::pair<uint, uint> ret = invalidcont;
		for (uint i = 0; i < sizes.size(); ++i)
		{
			sum += sizes[i];
			if (idx < sum) return { idx - sum + sizes[i], i };
		}
		return ret;
	}

	t* get(uint idx)
	{
		assert(idx < calcsizes());

		auto const& [idxrel, idxc] = container(idx);
		return getimpl(idxrel, idxc);
	}

	template<uint n = 0>
	t* getimpl(uint idx, uint idxc)
	{
		assert(idxc < mysize);
		if (n == idxc)
			return static_cast<t*>(&(std::get<n>(data)[idx]));
		if constexpr (n + 1 < mysize) return getimpl<n + 1>(idx, idxc);
		return nullptr;
	}

	template<uint n = 0>
	uint getsize(uint idxc) const
	{
		assert(idxc < mysize);

		if (n == idxc) return std::get<n>(data).size();
		if constexpr (n + 1 < mysize) return getsize<n + 1>(idxc);
		return 0;
	}

	std::array<uint, mysize> sizes;
	std::tuple<args_t&...> data;
};

template<typename t, typename... args_t>
auto makejoin(args_t&... _args) { return join<t, args_t...>(_args...); }
}