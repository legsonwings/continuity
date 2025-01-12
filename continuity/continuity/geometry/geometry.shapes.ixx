module;

#include "simplemath/simplemath.h"

export module geometry:shapes;

import stdxcore;
import graphicscore;
import std.core;

using namespace DirectX;
using vector3 = DirectX::SimpleMath::Vector3;
using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;

export namespace geometry
{

// todo : geoutils, create module parition
std::vector<gfx::vertex> create_cube(vector3 const& center, vector3 const& extents);
std::vector<vector3> create_box_lines(vector3 const& center, vector3 const& extents);
std::vector<vector3> create_cube_lines(vector3 const& center, float scale);

struct box
{
    box() = default;
    box(vector3 const& _center, vector3 const& _extents) : center(_center), extents(_extents) {}

    std::vector<vector3> vertices();
    vector3 center, extents;
};

struct aabb
{
    aabb() = default;
    aabb(vector3 const (&tri)[3]);

    aabb(std::vector<vector3> const& points);
    aabb(vector3 const* points, uint len);
    aabb(vector3 const& _min, vector3 const& _max) : min_pt(_min), max_pt(_max) {}

    float volume() const { auto s = span(); return s.x * s.y * s.z; }
    vector3 center() const { return (max_pt + min_pt) / 2.f; }
    vector3 span() const { return max_pt - min_pt; }
    operator box() const { return { center(), span() }; }
    aabb move(vector3 const& off) const { return aabb(min_pt + off, max_pt + off); }

    aabb& operator+=(vector3 const& pt);
    vector3 bound(vector3 const& pt) const;
    bool contains(vector3 const& pt) const;
    std::optional<aabb> intersect(aabb const& r) const;

    // top left front = min, bot right back = max
    vector3 min_pt;
    vector3 max_pt;
};

struct cube
{
    constexpr cube(vector3 const& _center, vector3 const& _extents) : center(_center), extents(_extents) {}

    aabb const& bbox() const;
    std::vector<gfx::vertex> vertices() const;
    std::vector<gfx::vertex> vertices_flipped() const;
    std::vector<gfx::instance_data> instancedata() const;
    vector3 const center, extents;
};

struct sphere
{
    sphere() = default;
    sphere(vector3 const& _center, float _radius, bool _gentris = true);

    void generate_triangles();

    std::vector<gfx::instance_data> instancedata() const;
    std::vector<gfx::vertex> const& vertices() const { stdx::cassert(triangulated_sphere.size() > 0, "Call generate_triangles() or pass generation flag to constructor."); return triangulated_sphere; }

    float radius = 1.5f;
    vector3 center = {};

    static constexpr uint numsegments_longitude = 8;
    static constexpr uint numsegments_latitude = (numsegments_longitude / 2) + (numsegments_longitude % 2);

private:

    void generate_triangles(std::vector<vector3> const& unitsphere_triangles);
    static void cacheunitsphere(uint numsegments_longitude, uint numsegments_latitude);
    static vector3 spherevertex(float const phi, float const theta) { return { std::sinf(theta) * std::cosf(phi), std::cosf(theta), std::sinf(theta) * std::sinf(phi) }; }

    std::vector<gfx::vertex> triangulated_sphere;

    // can't use std::unordered_map for some reason
    static inline std::map<uint, std::vector<vector3>> unitspheres_tessellated;
};

}