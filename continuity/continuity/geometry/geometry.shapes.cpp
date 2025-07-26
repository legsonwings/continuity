module;


#include "simplemath/simplemath.h"

// redefine XM_CALLCONV as fastcall since vectorcall seems to have problems with modules
#undef XM_CALLCONV
#define XM_CALLCONV __fastcall

module geometry:shapes;

import stdxcore;
import vec;
import std;

import graphics;

namespace geometry
{

using vector2 = DirectX::SimpleMath::Vector2;
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

        transformed_points[i] = { (pos + tx[1]), vector2{}, normal };
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

    auto const scale = extents / 2;

    std::vector<vector3> res;
    res.reserve(24);
    for (auto const v : unitcubelines) res.emplace_back(v.x * scale.x, v.y * scale.y, v.z * scale.z);
    return res;
}

std::vector<vector3> create_cube_lines(vector3 const& center, float scale)
{
    return create_box_lines(center, { scale, scale, scale });
}

std::vector<stdx::vec3> geometry::box::vertices()
{
    std::vector<stdx::vec3> verts;

    for (auto const& v : create_box_lines(vector3(center.data()), vector3(extents.data())))
    {
        // todo : create_box_lines to use vecx
        verts.push_back({ v[0], v[1], v[2]});
    }

    return verts;
}

aabb::aabb() {}

geometry::aabb::aabb(stdx::vec3 const (&tri)[3])
{
    stdx::vec3 const& v0 = tri[0];
    stdx::vec3 const& v1 = tri[1];
    stdx::vec3 const& v2 = tri[2];

    min_pt[0] = std::min({ v0[0], v1[0], v2[0] });
    max_pt[0] = std::max({ v0[0], v1[0], v2[0] });
    min_pt[1] = std::min({ v0[1], v1[1], v2[1] });
    max_pt[1] = std::max({ v0[1], v1[1], v2[1] });
    min_pt[2] = std::min({ v0[2], v1[2], v2[2] });
    max_pt[2] = std::max({ v0[2], v1[2], v2[2] });
}

aabb::aabb(std::vector<stdx::vec3> const& points) : aabb(points.data(), points.size()) {}

geometry::aabb::aabb(stdx::vec3 const* points, uint len)
{
    for (uint i = 0; i < len; ++i)
    {
        min_pt[0] = std::min(points[i][0], min_pt[0]);
        min_pt[1] = std::min(points[i][1], min_pt[1]);
        min_pt[2] = std::min(points[i][2], min_pt[2]);

        max_pt[0] = std::max(points[i][0], max_pt[0]);
        max_pt[1] = std::max(points[i][1], max_pt[1]);
        max_pt[2] = std::max(points[i][2], max_pt[2]);
    }
}

aabb& geometry::aabb::operator+=(stdx::vec3 const& pt)
{
    min_pt[0] = std::min(pt[0], min_pt[0]);
    min_pt[1] = std::min(pt[1], min_pt[1]);
    min_pt[2] = std::min(pt[2], min_pt[2]);

    max_pt[0] = std::max(pt[0], max_pt[0]);
    max_pt[1] = std::max(pt[1], max_pt[1]);
    max_pt[2] = std::max(pt[2], max_pt[2]);

    return *this;
}

stdx::vec3 aabb::bound(stdx::vec3 const& pt) const
{
    stdx::vec3 localpt = pt - center();
    stdx::vec3 halfextents = span() / 2.0f;
    return stdx::vec3{ std::min(std::abs(localpt[0]), halfextents[0]) * stdx::sign(localpt[0]), std::min(std::abs(localpt[0]), halfextents[0]) * stdx::sign(localpt[0]), std::min(std::abs(localpt[2]), halfextents[2]) * stdx::sign(localpt[2]) } + center();
    //stdx::vec3 halfextents = span() / 2.0f;
    //return stdx::clamp(localpt, -halfextents, halfextents);
}

bool aabb::contains(stdx::vec3 const& pt) const
{
    auto const& localpt = pt - center();
    auto const& halfextents = span() / 2.0f;
    return std::abs(localpt[0]) < halfextents[0] && std::abs(localpt[1]) < halfextents[1] && std::abs(localpt[2]) < halfextents[2];
}

std::optional<aabb> geometry::aabb::intersect(aabb const& r) const
{
    if (max_pt[0] < r.min_pt[0] || min_pt[0] > r.max_pt[0]) return {};
    if (max_pt[1] < r.min_pt[1] || min_pt[1] > r.max_pt[1]) return {};
    if (max_pt[2] < r.min_pt[2] || min_pt[2] > r.max_pt[2]) return {};

    stdx::vec3 const min = { std::max(min_pt[0], r.min_pt[0]), std::max(min_pt[1], r.min_pt[1]), std::max(min_pt[2], r.min_pt[2]) };
    stdx::vec3 const max = { std::min(max_pt[0], r.max_pt[0]), std::min(max_pt[1], r.max_pt[1]), std::min(max_pt[2], r.max_pt[2]) };

    return { {min, max} };
}


aabb const& cube::bbox() const
{
    auto createaabb = [](auto const& verts)
    {
        std::vector<stdx::vec3> positions;
        for (auto const& v : verts) { positions.push_back({ v.position[0], v.position[1], v.position[2] }); }
        return aabb(positions);
    };

    static const aabb box = createaabb(vertices());
    return box;
}

std::vector<gfx::vertex> cube::vertices() const
{
    return create_cube(vector3::Zero, extents);
}

std::vector<uint32> const& cube::indices() const
{
    static const std::vector<uint32> indices = []()
    {
        std::vector<uint32> indices;
        indices.reserve(36);
        for (auto i : stdx::range(36))
            indices.push_back(uint32(i));

        return indices;
    }();
    
    return indices;
}

std::vector<gfx::vertex> cube::vertices_flipped() const
{
    auto invert = [](auto const& verts)
    {
        std::vector<gfx::vertex> result;
        result.reserve(verts.size());

        // todo : this only works if centre is at origin
        // just flip the position to turn geometry inside out
        for (auto const& v : verts) { result.emplace_back(-v.position, vector2{}, v.normal); }

        return result;
    };

    static const std::vector<gfx::vertex> invertedvertices = invert(vertices());
    return invertedvertices;
}

std::vector<gfx::instance_data> cube::instancedata() const { return { gfx::instance_data(matrix::CreateTranslation(center), gfx::globalresources::get().view()) }; }

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

std::vector<gfx::instance_data> sphere::instancedata() const { return { gfx::instance_data(matrix::CreateTranslation(center), gfx::globalresources::get().view()) }; }

void sphere::generate_triangles(std::vector<vector3> const& unitsphere_triangles)
{
    triangulated_sphere.clear();
    triangulated_sphere.reserve(unitsphere_triangles.size());
    for (auto const& v : unitsphere_triangles)
        triangulated_sphere.emplace_back(v * radius + center, vector2{}, v);
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