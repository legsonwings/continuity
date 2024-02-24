module geometry;

import stdxcore;
import std.core;

vec2 to2d(vec3 const& v) { return { v[0], v[1] }; }

namespace geometry
{

constexpr line2d::line2d(vec2 const& _point, vec2 const& _dir) : point(_point), dir(_dir) {}

float line2d::getparameter(line2d const& line, vec2 const& point)
{
    // assuming point is already on the line
    auto origin = line.point;
    auto const topoint = (point - origin).normalized();

    return origin.distancesqr(point) * ((topoint.dot(line.dir)) > 0.f ? 1.f : -1.f);
}

constexpr linesegment::linesegment(vec3 const& _v0, vec3 const& _v1) : v0(_v0), v1(_v1) {}

std::optional<vec2> linesegment::intersect_line2d(linesegment const& linesegment, line const& line)
{
    auto cross = [](vec2 const& l, vec2 const& r) { return l[0] * r[1] - l[1] * r[0]; };
    // point = p + td1
    // point = q + ud2

    vec2 p{ to2d(linesegment.v0) }, q{ to2d(line.point) };
    vec2 d1(to2d(linesegment.v1 - linesegment.v0)), d2(to2d(line.dir));
    d1.normalized();

    if (float d1crossd2 = cross(d1, d2); d1crossd2 != 0.f)
    {
        float t = cross(q - p, d2) / d1crossd2;

        return p + t * d1;
    }

    return {};
}

line::line(vec3 const& _point, vec3 const& _dir) : point(_point), dir(_dir) {}

line2d line::to2d() const { return { ::to2d(point), ::to2d(dir)}; }

float line::distance(vec3 const& pt) const
{
    return pt.distance(closestpoint(pt), pt);
}

float line::distancesqr(vec3 const& pt) const
{
    return pt.distancesqr(closestpoint(pt));
}

vec3 line::closestpoint(vec3 const& pt) const
{
    return point + (pt - point).dot(dir) * dir;
}

std::optional<vec3> line::intersect_lines(line const& l, line const& r)
{
    // if the lines intersect they need to be on the same plane.
    // calculate plane normal
    auto const n = l.dir.cross(r.dir).normalized();

    // if lines intersect dot product to normal and any point on both lines is constant
    auto const c1 = n.dot(l.point);
    if (stdx::equals(c1, n.dot(r.point)))
        return {};

    // calculate cross of n and line dir
    auto const ldir_cross_n = l.dir.cross(n).normalized();

    // this constant should be same for all points on l, i.e 
    auto const c = ldir_cross_n.dot(l.point);

    // c = ldir_cross_n.Dot(isect) and isect = r.point + r.dir * t2
    auto const t2 = (c - ldir_cross_n.dot(r.point)) / ldir_cross_n.dot(r.dir);

    return { r.point + t2 * r.dir };
}

float line::getparameter(line const& line, vec3 const& point)
{
    // assuming point is already on the line
    auto origin = line.point;
    auto const topoint = (point - origin).normalized();

    return vec3::distance(origin, point) * (topoint.dot(line.dir) > 0.f ? 1.f : -1.f);
}
//
//box::box(vec3 const& _center, vec3 const& _extents) : center(_center), extents(_extents) {}
//
//aabb::aabb() : minpt({}), maxpt({}) {}
//aabb::aabb(vec3 const& _min, vec3 const& _max) : minpt(_min), maxpt(_max) {}
//
//geometry::aabb::aabb(vec3 const* points, uint len)
//{
//    minpt = vec3{ std::numeric_limits<float>::max() };
//    maxpt = vec3{ std::numeric_limits<float>::lowest() };
//
//    for (uint i = 0; i < len; ++i)
//    {
//        minpt[0] = std::min(points[i][0], minpt[0]);
//        minpt[1] = std::min(points[i][1], minpt[1]);
//        minpt[2] = std::min(points[i][2], minpt[2]);
//
//        maxpt[0] = std::max(points[i][0], maxpt[0]);
//        maxpt[1] = std::max(points[i][1], maxpt[1]);
//        maxpt[2] = std::max(points[i][2], maxpt[2]);
//    }
//}
//
//vec3 aabb::center() const
//{
//    return (maxpt + minpt) / 2.f;
//}
//
//vec3 aabb::span() const
//{
//    return maxpt - minpt;
//}
//
//aabb::operator box() const 
//{ 
//    return { center(), span() };
//}
//
//aabb aabb::move(vec3 const& off) const 
//{
//    return aabb(minpt + off, maxpt + off);
//}
//
//aabb& geometry::aabb::operator+=(vec3 const& pt)
//{
//    minpt[0] = std::min(pt[0], minpt[0]);
//    minpt[1] = std::min(pt[1], minpt[1]);
//    minpt[2] = std::min(pt[2], minpt[2]);
//
//    maxpt[0] = std::max(pt[0], maxpt[0]);
//    maxpt[1] = std::max(pt[1], maxpt[1]);
//    maxpt[2] = std::max(pt[2], maxpt[2]);
//
//    return *this;
//}
//
//std::optional<aabb> geometry::aabb::intersect(aabb const& r) const
//{
//    if (maxpt[0] < r.minpt[0] || minpt[0] > r.maxpt[0]) return {};
//    if (maxpt[1] < r.minpt[1] || minpt[1] > r.maxpt[1]) return {};
//    if (maxpt[2] < r.minpt[2] || minpt[2] > r.maxpt[2]) return {};
//
//    vec3 const min = { std::max(minpt[0], r.minpt[0]), std::max(minpt[1], r.minpt[1]), std::max(minpt[2], r.minpt[2]) };
//    vec3 const max = { std::min(maxpt[0], r.maxpt[0]), std::min(maxpt[1], r.maxpt[1]), std::min(maxpt[2], r.maxpt[2]) };
//
//    return { {min, max} };
//}

}