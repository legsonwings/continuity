module;

#include "simplemath/simplemath.h"

export module beziermaths;

import stdxcore;
import stdx;
import vec;
import std.core;

export namespace beziermaths
{

using vector2 = DirectX::SimpleMath::Vector2;
using vector3 = DirectX::SimpleMath::Vector3;
using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;
using plane = DirectX::SimpleMath::Plane;

using controlpoint = vector3;
using curveeval = std::pair<vector3, vector3>;
using voleval = std::pair<vector3, matrix>;
using vertex = std::pair<vector3, vector3>;

// these should be moved to geometry
// also need to create implement planar polygon
struct quad3 : public std::array<stdx::vec3, 4>
{
    std::array<stdx::vec3, 6> tris() const
    {
        auto const& lb = (*this)[0];
        auto const& rb = (*this)[1];
        auto const& rt = (*this)[2];
        auto const& lt = (*this)[3];
        return { lb, rb, rt, lb, rt, lt };
    }
};

struct quad2 : public std::array<stdx::vec2, 4>
{
    quad3 to3(uint dim, float v) const
    {
        quad3 r;
        for (uint i = 0; i < size(); ++i)
        {
            r[i][dim] = v;
            uint cd = 0;
            for (uint k = 0; k < 3; ++k)
            {
                if (k != dim)
                    r[i][k] = (*this)[i][cd++];
            }
        }

        return r;
    }
};

// LEGACY BEZIER CONSTRUCTS : ARE BEING REPLACED BY SINGLE GENERIC CLASS

// quadratic bezier basis
inline constexpr vector3 qbasis(float t)
{
    float const invt = 1.f - t;
    return vector3(invt * invt, 2.f * invt * t, t * t);
}

// quadratic bezier 1st derivative basis
inline constexpr vector3 dqbasis(float t) { return vector3(2 * (t - 1.f), 2.f - 4 * t, 2 * t); };

template<uint n>
struct beziercurve
{
    constexpr beziercurve() = default;
    constexpr vector3& operator[](uint index) { return controlnet[index]; }
    constexpr vector3 const& operator[](uint index) const { return controlnet[index]; }
    static constexpr uint numcontrolpts = n + 1;
    std::array<controlpoint, numcontrolpts> controlnet;
};

template<uint n>
struct beziersurface
{
    constexpr beziersurface() = default;
    constexpr vector3& operator[](uint index) { return controlnet[index]; }
    constexpr vector3 const& operator[](uint index) const { return controlnet[index]; }
    static constexpr uint numcontrolpts = (n + 1) * (n + 1);
    std::array<controlpoint, numcontrolpts> controlnet;
};

template<uint n>
struct beziertriangle
{
    constexpr beziertriangle() = default;
    constexpr vector3& operator[](uint index) { return controlnet[index]; }
    constexpr vector3 const& operator[](uint index) const { return controlnet[index]; }
    static constexpr uint numcontrolpts = ((n + 1) * (n + 2)) / 2;
    std::array<controlpoint, numcontrolpts> controlnet;
};

template<uint n>
struct beziervolume
{
    constexpr beziervolume() = default;
    constexpr vector3& operator[](uint index) { return controlnet[index]; }
    constexpr vector3 const& operator[](uint index) const { return controlnet[index]; }
    static constexpr uint numcontrolpts = (n + 1) * (n + 1) * (n + 1);
    std::array<controlpoint, numcontrolpts> controlnet;
};

template<uint n, uint s> requires(n >= 0 && n >= s)  // degree cannot be negative or smaller than subdivision end degree
struct decasteljau
{
    static beziercurve<s> curve(beziercurve<n> const& curve, float t)
    {
        beziercurve<n - 1u> subcurve;
        for (uint i = 0; i < subcurve.numcontrolpts; ++i)
            // linear interpolation to calculate subcurve controlnet
            subcurve.controlnet[i] = curve.controlnet[i] * (1 - t) + curve.controlnet[i + 1] * t;

        return decasteljau<n - 1u, s>::curve(subcurve, t);
    }

    static beziertriangle<s> triangle(beziertriangle<n> const& patch, vector3 const& uvw)
    {
        beziertriangle<n - 1u> subpatch;
        for (uint i = 0; i < subpatch.numcontrolpts; ++i)
        {
            // barycentric interpolation to calculate subpatch controlnet
            auto const& idx = stdx::triindex<n - 1u>::to3d(i);
            subpatch.controlnet[i] = patch[stdx::triindex<n>::to1d(idx.j, idx.k)] * uvw.x + patch[stdx::triindex<n>::to1d(idx.j + 1, idx.k)] * uvw.y + patch[stdx::triindex<n>::to1d(idx.j, idx.k + 1)] * uvw.z;
        }

        return decasteljau<n - 1u, s>::triangle(subpatch, uvw);
    }

    static beziersurface<s> surface(beziersurface<n> const& patch, vector2 const& uv)
    {
        static constexpr stdx::grididx<1> u0(10);
        static constexpr stdx::grididx<1> u1(1);

        auto const [u, v] = uv;
        beziersurface<n - 1u> subpatch;
        for (uint i = 0; i < subpatch.numcontrolpts; ++i)
        {
            // bilinear interpolation to calculate subpatch controlnet
            auto const& idx = stdx::grididx<1>::from1d(n - 1, i);
            subpatch.controlnet[i] = (patch[stdx::grididx<1>::to1d(n, idx)] * (1.f - u) + patch[stdx::grididx<1>::to1d(n, idx + u0)] * u) * (1.f - v)
                + (patch[stdx::grididx<1>::to1d(n, idx + u1)] * (1.f - u) + patch[stdx::grididx<1>::to1d(n, idx + u0 + u1)] * u) * v;
        }

        return decasteljau<n - 1u, s>::surface(subpatch, uv);
    }

    static beziervolume<s> volume(beziervolume<n> const& vol, vector3 const& uvw)
    {
        using cubeidx = stdx::grididx<2>;
        static constexpr auto u0 = cubeidx(100);
        static constexpr auto u1 = cubeidx(10);
        static constexpr auto u2 = cubeidx(100);

        auto const [t0, t2, t1] = uvw;
        beziervolume<n - 1u> subvol;
        for (uint i = 0; i < subvol.numcontrolpts; ++i)
        {
            auto const& idx = cubeidx::from1d(n - 1, i);

            // trilinear intepolation to calculate subvol controlnet
            // 100 is the first edge(edge with end point at 100) along x(first dimension)
            // 8 cube verts from 000 to 111
            vector3 const c100 = vol[cubeidx::to1d(n, idx)] * (1.f - t0) + vol[cubeidx::to1d(n, idx + u0)] * t0;
            vector3 const c110 = vol[cubeidx::to1d(n, idx + u1)] * (1.f - t0) + vol[cubeidx::to1d(n, idx + u0 + u1)] * t0;
            vector3 const c101 = vol[cubeidx::to1d(n, idx + u2)] * (1.f - t0) + vol[cubeidx::to1d(n, idx + u0 + u2)] * t0;
            vector3 const c111 = vol[cubeidx::to1d(n, idx + u1 + u2)] * (1.f - t0) + vol[cubeidx::to1d(n, idx + u0 + u1 + u2)] * t0;

            subvol.controlnet[i] = (c100 * (1 - t1) + c110 * t1) * (1 - t2) + (c101 * (1 - t1) + c111 * t1) * t2;
        }

        return decasteljau<n - 1u, s>::volume(subvol, uvw);
    }
};

template<uint n>
requires (n >= 0)
struct decasteljau<n, n>
{
    static constexpr beziercurve<n> curve(beziercurve<n> const& curve, float t) { return curve; }
    static constexpr beziertriangle<n> triangle(beziertriangle<n> const& patch, vector3 const& uvw) { return patch; }
    static constexpr beziersurface<n> surface(beziersurface<n> const& patch, vector2 const& uv) { return patch; }
    static constexpr beziervolume<n> volume(beziervolume<n> const& vol, vector3 const& uvw) { return vol; }
};

template<uint n>
requires (n >= 0)
constexpr curveeval evaluate(beziercurve<n> const& curve, float t)
{
    auto const& line = decasteljau<n, 1>::curve(curve, t);
    return { line.controlnet[0] * (1 - t) + line.controlnet[1] * t, (line.controlnet[1] - line.controlnet[0]).Normalized() };
};

template<uint n>
constexpr vertex evaluate(beziertriangle<n> const& patch, vector3 const& uvw)
{
    auto const& triangle = decasteljau<n, 1>::triangle(patch, uvw);
    controlpoint const& p010 = triangle[stdx::triindex<n>::to1d(1, 0)];
    controlpoint const& p100 = triangle[stdx::triindex<n>::to1d(0, 0)];
    controlpoint const& p001 = triangle[stdx::triindex<n>::to1d(0, 1)];

    return { { p100 * uvw.x + p010 * uvw.y + p001 * uvw.z }, (p100 - p010).Cross(p001 - p010).Normalized() };
}

template<uint n>
constexpr vertex evaluate(beziersurface<n> const& vol, vector2 const& uv)
{
    auto const& square = decasteljau<n, 1>::surface(vol, uv);
    // todo : which constructor does this invoke? could be ambigous
    controlpoint const& p00 = square[stdx::grididx<1>::to1d(1, 0)];
    controlpoint const& p10 = square[stdx::grididx<1>::to1d(1, 10)];
    controlpoint const& p01 = square[stdx::grididx<1>::to1d(1, 1)];

    return { decasteljau<1, 0>::surface(square, uv).controlnet[0], (p01 - p00).Cross(p10 - p00).Normalized() };
};

template<uint n>
requires(n >= 0)
constexpr vector3 evaluate(beziervolume<n> const& vol, vector3 const& uwv) { return decasteljau<n, 0>::volume(vol, uwv).controlnet[0]; };

template<uint n>
requires(n >= 0)
constexpr vector3 evaluatefast(beziervolume<n> const& vol, vector3 const& uwv) { return decasteljau<n, 0>::volume(vol, uwv).controlnet[0]; };

voleval evaluatefast(beziervolume<2> const& v, vector3 const& uwv);
std::vector<vertex> bulkevaluate(beziervolume<2> const& v, std::vector<vertex> const& vertices);

template<uint n>
constexpr beziertriangle<n + 1> elevate(beziertriangle<n> const& patch)
{
    beziertriangle<n + 1> elevatedPatch;
    for (int i = 0; i < elevatedPatch.numcontrolpts; ++i)
    {
        auto const& idx = stdx::triindex<n + 1>::to3d(i);
        // subtraction will yield negative indices at times ignore such points
        auto const& term0 = idx.i == 0 ? vector3::Zero : patch.controlnet[stdx::triindex<n>::to1d(idx.j, idx.k)] * idx.i;
        auto const& term1 = idx.j == 0 ? vector3::Zero : patch.controlnet[stdx::triindex<n>::to1d(idx.j - 1, idx.k)] * idx.j;
        auto const& term2 = idx.k == 0 ? vector3::Zero : patch.controlnet[stdx::triindex<n>::to1d(idx.j, idx.k - 1)] * idx.k;
        elevatedPatch.controlnet[i] = (term0 + term1 + term2) / (n + 1);
    }
    return elevatedPatch;
}

// LEGACY BEZIER CONSTRUCTS

// n-variate bezier of degree d, with hyper cubic domain
template<uint n, uint d>
requires(n >= 0 && d >= 0)
struct planarbezier
{
    using param_t = stdx::vec<n>;
    using bezier_t = planarbezier<n, d>;
    using eval_t = std::array<vector3, n + 1>;

    constexpr controlpoint& operator[](uint index) { return controlnet[index]; }
    constexpr controlpoint const& operator[](uint index) const { return controlnet[index]; }
    constexpr operator controlpoint() requires(d == 0) { return controlnet[0]; }

    static constexpr uint numcontrolpts = stdx::pown((d + 1), n);
    
    // todo : use stdx::vec3
    std::array<controlpoint, numcontrolpts> controlnet;

    static constexpr bezier_t identity(float dist)
    {
        bezier_t r;
        for (auto i : stdx::range(numcontrolpts))
        {
            stdx::vec3 pos{};
            auto const idx = stdx::grididx<n - 1>::from1d(d, i);
            for (auto c : stdx::range(std::min(n, (uint)3)))
                pos[c] = idx[c] * dist;

            r[i] = { pos[0], pos[1], pos[2] };
        }

        return r;
    }

    template<uint s>
    requires(d >= s)  // degree cannot be negative or smaller than subdivision end degree
    static constexpr planarbezier<n, s> decasteljau(bezier_t const& b, bezier_t::param_t const& p) { return realdecasteljau<d, s>::apply(b, p); }

    //                (2,2) = (1,1) + (1,1)                               
    // ┌──────┬──────┐(1,1)
    // │      │      │
    // │      │(1,1) │
    // │      │(0,0) │
    // ├──────x──────┤
    // │      │      │
    // │      │      │
    // │      │      │
    // o──────┴──────┘
    // (0,0)
    // x is the new basis for the cell, to change basis to o from x just add
    static constexpr controlpoint computesubcontrolpoint(uint subidx, planarbezier<n, d> const& b, typename planarbezier<n, d>::param_t const& p)
    {
        using grididx = stdx::grididx<n - 1>;

        // controlpoint idx(nd) for start of hypercube that determines subcontrolpoint is same as subcontrolpoint index
        auto const index = grididx::from1d(d - 1, subidx);

        std::array<controlpoint, stdx::pown(2, n)> r;
        for (uint i(0); i < r.size(); ++i)
            r[i] = b[grididx::to1d(d, grididx::from1d(1, i) + index)];

        return stdx::nlerp<controlpoint, n - 1, 0>::lerp(r, p);
    }

    // we can optimize this using simd
    // todo : can recursion be avoided and still the evaluations kept generic
    constexpr std::vector<eval_t> eval(std::vector<param_t> const& params)
    {
        std::vector<eval_t> r;
        for (auto const& p : params) r.push_back(eval(p));
        return r;
    }

    // todo : probably never constepxr unless we use std::vec3
    // todo : implement stdx::matrix
    constexpr eval_t eval(param_t const& param) const
    {
        eval_t res;
        auto const& subbezier = decasteljau<1>(*this, param);
        res[0] = planarbezier<n, 1>::decasteljau<0>(subbezier, param);

        if constexpr (n == 0)
            return res;

        // the generic expression doesn't work for first dimension
        res[1] = vector3(subbezier[1] - subbezier[0]).Normalized();
        for (uint i(2); i <= n; ++i)
            res[i] = vector3(subbezier[(i - 1) * 2] - subbezier[0]).Normalized();

        return res;
    }

private:

    template<uint d, uint s>
    requires(d >= 0 && d >= s)  // degree cannot be negative or smaller than subdivision end degree
    struct realdecasteljau
    {
        using bezier_t = planarbezier<n, d>;
        static planarbezier<n, s> apply(bezier_t const& b, bezier_t::param_t const& p)
        {
            planarbezier<n, d - 1> subbezier;
            for (uint i(0); i < subbezier.numcontrolpts; ++i)
                subbezier[i] = b.computesubcontrolpoint(i, b, p);

            return realdecasteljau<d - 1, s>::apply(subbezier, p);
        }
    };

    template<uint d>
    struct realdecasteljau<d, d>
    {
        using bezier_t = planarbezier<n, d>;
        static bezier_t apply(bezier_t const& b, bezier_t::param_t const& p) { return b; }
    };
};

template<uint d>
auto tessellateboundary(planarbezier<3, d> const& bezier, uint intervals)
{
    using b_t = planarbezier<3, d>;
    std::pair<std::vector<typename b_t::param_t>, std::vector<vector3>> r;
    float const step = 1.f / intervals;
    uint const nfacecells = stdx::pown(intervals, 2);
    for (uint j(0); j < nfacecells; ++j)
    {
        stdx::vec2 const lb = stdx::grididx<1>::from1d(intervals - 1, j).castas<float>() / float(intervals);
        auto const rb = lb + stdx::vec2{ step, 0.f };
        auto const rt = rb + stdx::vec2{ 0.f, step };
        auto const lt = lb + stdx::vec2{ 0.f, step };

        quad2 q{ lb, rb, rt, lt };
        for (uint i = 0; i < 3; ++i)
        {
            for (auto v : q.to3(i, 0.f).tris()) r.first.push_back(v);
            for (auto v : q.to3(i, 1.f).tris()) r.first.push_back(v);

            r.second.push_back(-vector3(i));
            r.second.push_back(vector3(i));
        }
    }

    return r;
}

//
//template<uint d>
//auto tessellateboundary(planarbezier<3, d> const& bezier, uint intervals)
//{
//    using b_t = planarbezier<3, d>;
//    std::vector<typename b_t::eval_t> r;
//    uint const nfacecells = stdx::pown(intervals, 2);
//    
//    float const step = 1.f / intervals;
//    for (uint i = 0; i < 3; ++i)
//    {
//        for (uint j(0); j < nfacecells; ++j)
//        {
//            stdx::vec2 const lb = stdx::grididx<1>::from1d(intervals - 1, j).castas<float>() / float(intervals);
//            auto const rb = lb + stdx::vec2{step, 0.f};
//            auto const rt = rb + stdx::vec2{0.f, step};
//            auto const lt = lb + stdx::vec2{0.f, step};
//
//            quad2 q{ lb, rb, rt, lt };
//            auto tris = q.to3(i, 0.f).tris();
//            for (auto v : q.to3(i, 0.f).tris())
//            {
//                auto eval = bezier.eval(v);
//                eval[1] = -eval[i + 1];
//                r.push_back(eval);
//            }
//
//            /*for (int k(tris.size() - 1); k >= 0; --k)
//            {
//                auto eval = bezier.eval(tris[k]);
//                eval[1] = -eval[i + 1];
//                r.push_back(eval);
//            }*/
//            for (auto v : q.to3(i, 1.f).tris())
//            {
//                auto eval = bezier.eval(v);
//                eval[1] = eval[i + 1];
//                r.push_back(eval);
//            }
//        }
//    }
//
//    return r;
//}

template<uint n, uint d>
auto tessellate(planarbezier<n, d> const& bezier, uint intervals)
{
    std::vector<typename planarbezier<n, d>::eval_t> r;

    uint const nsteps = stdx::pown(intervals + 1, n);
    for (uint i(0); i < nsteps; ++i)
    {
        auto const p = stdx::grididx<n - 1>::from1d(intervals - 1, i).castas<float>() / float(intervals);
        r.push_back(bezier.eval(p));
    }

    return r;
}

bool firstclosest(vector3 p, vector3 p0, vector3 p1)
{
    return vector3::DistanceSquared(p, p0) < vector3::DistanceSquared(p, p1);
}

using paramandpoint = std::pair<float, vector3>;
paramandpoint closestpoint(planarbezier<1, 2> const& c, vector3 p)
{
    static constexpr int maxitr = 4;
    float tx = 0.5f;
    float ty = 0.5f;

    vector3 xeval, yeval, xdeval, ydeval;
    for (uint i(0); i < maxitr; ++i)
    {
        auto const xeval = c.eval({ tx });
        auto const yeval = c.eval({ ty });

        tx = tx - ((p.x - xeval[0].x) / -xeval[1].x);
        ty = ty - ((p.y - yeval[0].y) / -yeval[1].y);
    }

    if (!stdx::equals(tx, ty))
        return firstclosest(p, xeval, yeval) ? paramandpoint{tx, xeval} : paramandpoint{ty, yeval};
    
    return { tx, xeval };
}

}