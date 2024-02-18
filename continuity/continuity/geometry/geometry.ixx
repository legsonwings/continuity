export module geometry;

import stdxcore;
import vec;
import std.core;

using vec2 = stdx::vec2;
using vec3 = stdx::vec3;

export namespace geometry
{

template<typename t>
struct tessellatable
{
	template<typename ...args_t>
	auto tessellate(args_t&&... args) const
	{
		return tessellate(*static_cast<const t*>(this), std::forward<args_t>(args)...);
	}
};

struct line2d
{
    line2d() = default;
    constexpr line2d(vec2 const& _point, vec2 const& _dir);

    static float getparameter(line2d const& line, vec2 const& point);

    vec2 point, dir;
};

struct line
{
    line() = default;

    // todo : can't have this as constexpr for some reason
    // vec needs to have constexpr constructor
    line(vec3 const& _point, vec3 const& _dir);

    line2d to2d() const;

    float distance(vec3 const& pt) const;
    float distancesqr(vec3 const& pt) const;
    vec3 closestpoint(vec3 const& pt) const;
    static float getparameter(line const& line, vec3 const& point);
    static std::optional<vec3> intersect_lines(line const& l, line const& r);

    vec3 point, dir;
};

struct linesegment
{
    linesegment() = default;
    constexpr linesegment(vec3 const& _v0, vec3 const& _v1) : v0(_v0), v1(_v1) {}

    static std::optional<vec2> intersect_line2d(linesegment const& linesegment, line const& line);

    vec3 v0, v1;
};

// todo : can't do much until stdx::matrix is implmented with std::matrix::determinant()
//struct triangle2D
//{
//    triangle2D() = default;
//    constexpr triangle2D(vector2 const& _v0, vector2 const& _v1, vector2 const& _v2) : v0(_v0), v1(_v1), v2(_v2) {}
//
//    bool isin(vector2 const& point) const
//    {
//        vector2 vec0 = v1 - v0, vec1 = v2 - v0, vector2 = point - v0;
//        float den = vec0.x * vec1.y - vec1.x * vec0.y;
//        float v = (vector2.x * vec1.y - vec1.x * vector2.y) / den;
//        float w = (vec0.x * vector2.y - vector2.x * vec0.y) / den;
//        float u = 1.0f - v - w;
//
//        return (u >= 0.f && u <= 1.f) && (v >= 0.f && v <= 1.f) && (w >= 0.f && w <= 1.f);
//    }
//
//    vector2 v0, v1, v2;
//};
//
//struct triangle
//{
//    triangle() = default;
//    constexpr triangle(vec3 const& _v0, vec3 const& _v1, vec3 const& _v2) : verts{ _v0, _v1, _v2 } {}
//
//    operator plane() const { return { verts[0], verts[1], verts[1] }; }
//
//    static std::optional<linesegment> intersect(triangle const& t0, triangle const& t1) { return intersect(t0.verts, t1.verts); }
//
//    static std::optional<linesegment> intersect(vec3 const* t0, vec3 const* t1)
//    {
//        vec3 const t0_normal = (t0[1] - t0[0]).Cross(t0[2] - t0[0]).Normalized();
//        vec3 const t1_normal = (t1[1] - t1[0]).Cross(t1[2] - t1[0]).Normalized();
//
//        // construct third plane as cross product of the two plane normals and passing through origin
//        auto const linedir = t0_normal.Cross(t1_normal).Normalized();
//
//        // use 3 plane intersection to find a point on the inerseciton line
//        auto const det = matrix(t0_normal, t1_normal, linedir).Determinant();
//
//        if (fabs(det) < FLT_EPSILON)
//            return {};
//
//        vec3 const& linepoint0 = (t0[0].Dot(t0_normal) * (t1_normal.Cross(linedir)) + t1[0].Dot(t1_normal) * linedir.Cross(t0_normal)) / det;
//
//        line const isect_line{ linepoint0, linedir };
//
//        std::vector<line> edges;
//        edges.reserve(6);
//
//        edges.emplace_back(line{ t0[0], (t0[1] - t0[0]).Normalized() });
//        edges.emplace_back(line{ t0[1], (t0[2] - t0[1]).Normalized() });
//        edges.emplace_back(line{ t0[2], (t0[0] - t0[2]).Normalized() });
//
//        edges.emplace_back(line{ t1[0], (t1[1] - t1[0]).Normalized() });
//        edges.emplace_back(line{ t1[1], (t1[2] - t1[1]).Normalized() });
//        edges.emplace_back(line{ t1[2], (t1[0] - t1[2]).Normalized() });
//
//        std::vector<vec3> isect_linesegment;
//        isect_linesegment.reserve(2);
//
//        for (auto const& edge : edges)
//        {
//            // ignore intersections with parallel edge
//            if (geoutils::nearlyequal(std::fabsf(edge.dir.Dot(isect_line.dir)), 1.f))
//                continue;
//
//            // the points on intersection of two triangles will be on both triangles
//            if (auto isect = line::intersect_lines(edge, isect_line); isect.has_value() && triangle::isin(t0, isect.value()) && triangle::isin(t1, isect.value()))
//                isect_linesegment.push_back(isect.value());
//        }
//
//        // remove duplicates, naively, maybe improve this
//        for (uint i = 0; i < std::max(uint(1), isect_linesegment.size()) - 1; ++i)
//            for (uint j = i + 1; j < isect_linesegment.size(); ++j)
//                if (geoutils::nearlyequal(isect_linesegment[i], isect_linesegment[j]))
//                {
//                    std::swap(isect_linesegment[j], isect_linesegment[isect_linesegment.size() - 1]);
//                    isect_linesegment.pop_back();
//                    j--;
//                }
//
//        if (isect_linesegment.size() < 2)
//            return {};
//
//        // bug : this can sometimes be invalid, only when frame time is high?
//        // assert(isect_linesegment.size() == 2);
//
//        return { { isect_linesegment[0], isect_linesegment[1] } };
//    }
//
//    static bool isin(vec3 const* tri, vec3 const& point)
//    {
//        vec3 const normal_unnormalized = (tri[1] - tri[0]).Cross(tri[2] - tri[0]);
//        vec3 const normal = normal_unnormalized.Normalized();
//
//        float const triarea = normal_unnormalized.Length();
//        if (triarea == 0)
//            return false;
//
//        float const alpha = (tri[1] - point).Cross(tri[2] - point).Length() / triarea;
//        float const beta = (tri[2] - point).Cross(tri[0] - point).Length() / triarea;
//
//        return alpha > -stdx::tolerance<float> && beta > -stdx::tolerance<float> && (alpha + beta) < (1.f + stdx::tolerance<float>);
//    }
//
//    // make this std::array
//    vec3 verts[3];
//};

struct box : public tessellatable<box>
{
    box() = default;
    box(vec3 const& _center, vec3 const& _extents);

    vec3 center, extents;
};

struct aabb
{
    aabb();
    aabb(std::span<vec3> const& points) : aabb(points.data(), points.size()) {}
    aabb(vec3 const* points, uint len);
    aabb(vec3 const& _min, vec3 const& _max);

    vec3 center() const;
    vec3 span() const;
    operator box() const;
    aabb move(vec3 const& off) const;

    aabb& operator+=(vec3 const& pt);
    std::optional<aabb> intersect(aabb const& r) const;

    // top left front = min, bot right back = max
    vec3 minpt;
    vec3 maxpt;
};


}