module;


#include "simplemath/simplemath.h"

// redefine XM_CALLCONV as fastcall since vectorcall seems to have problems with modules
#undef XM_CALLCONV
#define XM_CALLCONV __fastcall

module geometry:shapes;

import stdxcore;
import vec;
import std.core;

import graphics;

namespace geometry
{

using vector3 = DirectX::SimpleMath::Vector3;
using vector4 = DirectX::SimpleMath::Vector4;

constexpr vector3 unit_frontface_quad[4] =
{
    { -1.f, 1.f, 0.f },
    { 1.f, 1.f, 0.f },
    { 1.f, -1.f, 0.f },
    { -1.f, -1.f, 0.f }
};

constexpr vector3 unitcube[8] =
{
    { -1.f, 1.f, -1.f },
    { 1.f, 1.f, -1.f },
    { 1.f, -1.f, -1.f },
    { -1.f, -1.f, -1.f },
    { -1.f, 1.f, 1.f },
    { 1.f, 1.f, 1.f },
    { 1.f, -1.f, 1.f },
    { -1.f, -1.f, 1.f }
};

std::array<gfx::vertex, 4> transform_unitquad(const vector3(&verts)[4], const vector3(&tx)[3])
{
    static vector3 unitquad_normal = { 0.f, 0.f, -1.f };
    std::array<gfx::vertex, 4> transformed_points;

    auto const scale = tx[2];
    auto const angle = XMConvertToRadians(tx[0].Length());
    auto const axis = tx[0].Normalized();

    for (uint i = 0; i < 4; ++i)
    {
        vector3 normal;
        vector3 pos = { verts[i].x * scale.x, verts[i].y * scale.y, verts[i].z * scale.z };
        vector3::Transform(pos, DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(axis, angle), pos);
        vector3::Transform(unitquad_normal, DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(axis, angle), normal);

        transformed_points[i] = { (pos + tx[1]), normal };
    }

    return transformed_points;
};

std::vector<gfx::vertex> create_cube(vector3 const& center, vector3 const& extents)
{
    auto const scale = vector3{ extents.x / 2.f, extents.y / 2.f, extents.z / 2.f };
    vector3 transformations[6][3] =
    {
        {{0.f, 180.f, 0.f}, {0.f, 0.0f, scale.z}, {scale}},
        {{0.f, 360.f, 0.f}, {0.f, 0.0f, -scale.z}, {scale}},
        {{90.f, 0.f, 0.f}, {0.f, scale.y, 0.f}, {scale}},
        {{-90.f, 0.f, 0.f}, {0.f, -scale.y, 0.f}, {scale}},
        {{0.f, -90.f, 0.f}, {scale.x, 0.f, 0.f}, {scale}},
        {{0.f, 90.f, 0.f}, {-scale.x, 0.0f, 0.f}, {scale}}
    };

    std::vector<gfx::vertex> triangles;
    for (auto const& transformation : transformations)
    {
        auto const face = transform_unitquad(unit_frontface_quad, transformation);
        stdx::cassert(face.size() == 4);
        triangles.emplace_back(face[0]);
        triangles.emplace_back(face[1]);
        triangles.emplace_back(face[2]);
        triangles.emplace_back(face[2]);
        triangles.emplace_back(face[3]);
        triangles.emplace_back(face[0]);
    }

    for (auto& vertex : triangles) { vertex.position += center; }
    return triangles;
}

std::vector<vector3> create_box_lines(vector3 const& center, vector3 const& extents)
{
    auto createcubelines = [](vector3 const (&cube)[8])
    {
        std::vector<vector3> res;
        res.resize(24);

        for (uint i(0); i < 4; ++i)
        {
            // front face lines
            res[i * 2] = cube[i];
            res[i * 2 + 1] = cube[(i + 1) % 4];

            // back face lines
            res[i * 2 + 8] = cube[i + 4];
            res[i * 2 + 8 + 1] = cube[(i + 1) % 4 + 4];

            // lines connecting front and back faces
            res[16 + i * 2] = cube[i];
            res[16 + i * 2 + 1] = cube[i + 4];
        }

        return res;
    };

    static const std::vector<vector3> unitcubelines = createcubelines(unitcube);

    auto const scale = vector3{ extents.x / 2.f, extents.y / 2.f, extents.z / 2.f };

    std::vector<vector3> res;
    res.reserve(24);
    for (auto const v : unitcubelines) res.emplace_back(v.x * scale.x, v.y * scale.y, v.z * scale.z);
    return res;
}

std::vector<vector3> create_cube_lines(vector3 const& center, float scale)
{
    return create_box_lines(center, { scale, scale, scale });
}

std::vector<vector3> geometry::box::vertices() { return create_box_lines(center, extents); }

geometry::aabb::aabb(vector3 const (&tri)[3])
{
    vector3 const& v0 = tri[0];
    vector3 const& v1 = tri[1];
    vector3 const& v2 = tri[2];

    min_pt.x = std::min({ v0.x, v1.x, v2.x });
    max_pt.x = std::max({ v0.x, v1.x, v2.x });
    min_pt.y = std::min({ v0.y, v1.y, v2.y });
    max_pt.y = std::max({ v0.y, v1.y, v2.y });
    min_pt.z = std::min({ v0.z, v1.z, v2.z });
    max_pt.z = std::max({ v0.z, v1.z, v2.z });
}

aabb::aabb(std::vector<vector3> const& points) : aabb(points.data(), points.size()) {}

geometry::aabb::aabb(vector3 const* points, uint len)
{
    min_pt = vector3{ std::numeric_limits<float>::max() };
    max_pt = vector3{ std::numeric_limits<float>::lowest() };

    for (uint i = 0; i < len; ++i)
    {
        min_pt.x = std::min(points[i].x, min_pt.x);
        min_pt.y = std::min(points[i].y, min_pt.y);
        min_pt.z = std::min(points[i].z, min_pt.z);

        max_pt.x = std::max(points[i].x, max_pt.x);
        max_pt.y = std::max(points[i].y, max_pt.y);
        max_pt.z = std::max(points[i].z, max_pt.z);
    }
}

aabb& geometry::aabb::operator+=(vector3 const& pt)
{
    min_pt.x = std::min(pt.x, min_pt.x);
    min_pt.y = std::min(pt.y, min_pt.y);
    min_pt.z = std::min(pt.z, min_pt.z);

    max_pt.x = std::max(pt.x, max_pt.x);
    max_pt.y = std::max(pt.y, max_pt.y);
    max_pt.z = std::max(pt.z, max_pt.z);

    return *this;
}

vector3 aabb::bound(vector3 const& pt) const
{
    auto const& localpt = pt - center();
    auto const& halfextents = span() / 2.0f;

    return vector3{ std::min(std::abs(localpt.x), halfextents.x) * stdx::sign(localpt.x), std::min(std::abs(localpt.y), halfextents.y) * stdx::sign(localpt.y), std::min(std::abs(localpt.z), halfextents.z) * stdx::sign(localpt.z) } + center();
    // can use this once switch to stdx::vec is made
    //return stdx::clamp(stdx::abs(localpt), halfextents) * stdx::sign(localpt);
}

bool aabb::contains(vector3 const& pt) const
{
    auto const& localpt = pt - center();
    auto const& halfextents = span() / 2.0f;
    return std::abs(localpt.x) <= halfextents.x && std::abs(localpt.y) <= halfextents.y && std::abs(localpt.z) <= halfextents.z;   
}

std::optional<aabb> geometry::aabb::intersect(aabb const& r) const
{
    if (max_pt.x < r.min_pt.x || min_pt.x > r.max_pt.x) return {};
    if (max_pt.y < r.min_pt.y || min_pt.y > r.max_pt.y) return {};
    if (max_pt.z < r.min_pt.z || min_pt.z > r.max_pt.z) return {};

    vector3 const min = { std::max(min_pt.x, r.min_pt.x), std::max(min_pt.y, r.min_pt.y), std::max(min_pt.z, r.min_pt.z) };
    vector3 const max = { std::min(max_pt.x, r.max_pt.x), std::min(max_pt.y, r.max_pt.y), std::min(max_pt.z, r.max_pt.z) };

    return { {min, max} };
}


aabb const& cube::bbox() const
{
    auto createaabb = [](auto const& verts)
    {
        std::vector<vector3> positions;
        for (auto const& v : verts) { positions.push_back(v.position); }
        return aabb(positions);
    };

    static const aabb box = createaabb(vertices());
    return box;
}

std::vector<gfx::vertex> cube::vertices() const
{
    return create_cube(vector3::Zero, extents);
}

std::vector<gfx::vertex> cube::vertices_flipped() const
{
    auto invert = [](auto const& verts)
    {
        std::vector<gfx::vertex> result;
        result.reserve(verts.size());

        // todo : this only works if centre is at origin
        // just flip the position to turn geometry inside out
        for (auto const& v : verts) { result.emplace_back(-v.position, v.normal); }

        return result;
    };

    static const std::vector<gfx::vertex> invertedvertices = invert(vertices());
    return invertedvertices;
}

std::vector<gfx::instance_data> cube::instancedata() const { return { gfx::instance_data(matrix::CreateTranslation(center), gfx::globalresources::get().view(), gfx::globalresources::get().mat("")) }; }

stdx::vec3 tovec3(vector3 const & v)
{
    return { v.x, v.y, v.z };
}

bool equals(vector3 const& l, vector3 const& r)
{
    // test speeds using vector3 const & vs pass by value
    return stdx::equals(tovec3(l), tovec3(r));
}

sphere::sphere(vector3 const& _center, float _radius, bool _gentris) : center(_center), radius(_radius)
{
    if (_gentris)
        generate_triangles();
}

void sphere::generate_triangles()
{
    if (unitspheres_tessellated[numsegments_longitude].size() == 0)
        sphere::cacheunitsphere(numsegments_longitude, numsegments_latitude);

    generate_triangles(unitspheres_tessellated[numsegments_longitude]);
}

std::vector<gfx::instance_data> sphere::instancedata() const { return { gfx::instance_data(matrix::CreateTranslation(center), gfx::globalresources::get().view(), gfx::globalresources::get().mat("")) }; }

void sphere::generate_triangles(std::vector<vector3> const& unitsphere_triangles)
{
    triangulated_sphere.clear();
    triangulated_sphere.reserve(unitsphere_triangles.size());
    for (auto const& v : unitsphere_triangles)
        triangulated_sphere.emplace_back(v * radius + center, v);
}

void sphere::cacheunitsphere(uint numsegments_longitude, uint numsegments_latitude)
{
    float const stepphi = XM_2PI / numsegments_longitude;
    float const steptheta = XM_PI / numsegments_latitude;
    uint const num_vertices = numsegments_longitude * numsegments_latitude * 4;

    auto& unitsphere = unitspheres_tessellated[numsegments_longitude];
    unitsphere.clear();
    unitsphere.reserve(num_vertices);

    float const theta_end = XM_PI - steptheta + stdx::tolerance<float>;
    float const phi_end = XM_2PI - stepphi + stdx::tolerance<float>;

    // divide the sphere into quads for each region defined by four circles(at theta, theta + step, phi and phi + step)
    for (float theta = 0.f; theta < theta_end; theta += steptheta)
        for (float phi = 0.f; phi < phi_end; phi += stepphi)
        {
            vector3 lbv, ltv, rbv, rtv;
            ltv = spherevertex(phi, theta);
            rtv = spherevertex(phi + stepphi, theta);
            lbv = spherevertex(phi, theta + steptheta);
            rbv = spherevertex(phi + stepphi, theta + steptheta);

            if (!equals(lbv, ltv) && !equals(lbv, rtv) && !equals(ltv, rtv))
            {
                unitsphere.push_back(lbv);
                unitsphere.push_back(ltv);
                unitsphere.push_back(rtv);
            }

            if (!equals(lbv, rtv) && !equals(lbv, rbv) && !equals(rtv, rbv))
            {
                unitsphere.push_back(lbv);
                unitsphere.push_back(rtv);
                unitsphere.push_back(rbv);
            }
        }
}

}