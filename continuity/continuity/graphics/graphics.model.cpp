module;

#include "thirdparty/rapidobj.hpp"
#include "simplemath/simplemath.h"

module graphics:model;

import :globalresources;

namespace gfx
{

model::model(std::string const& objpath, bool translatetoorigin)
{
    rapidobj::Result result = rapidobj::ParseFile(std::filesystem::path(objpath), rapidobj::MaterialLibrary::Default(rapidobj::Load::Optional));
    stdx::cassert(!result.error);

    rapidobj::Triangulate(result);
    stdx::cassert(!result.error);

    auto const& positions = result.attributes.positions;
    auto const& normals = result.attributes.normals;

    auto const numverts = positions.size() / 3;

    _vertices.resize(numverts);

    for (auto const& shape : result.shapes)
    {
        auto const& objindices = shape.mesh.indices;
        for (uint i(0); i < shape.mesh.num_face_vertices.size(); ++i)
        {
            stdx::cassert(shape.mesh.num_face_vertices[i] == 3);
            for (auto j : stdx::range(3u))
            {
                auto vindex = objindices[i * 3 + j].position_index;
                _indices.push_back(vindex);

                // index into flat array of floats
                auto posarridx = vindex * 3;

                _vertices[vindex].position = vector3(positions[posarridx], positions[posarridx + 1], positions[posarridx + 2]);

                if (normals.size() > 0)
                {
                    // normals may be shared by multiple vertices, so add them inline in each gfx::vertex
                    auto normarridx = objindices[i * 3 + j].normal_index * 3;
                    stdx::cassert(normarridx < normals.size());
                    _vertices[vindex].normal = vector3(normals[normarridx], normals[normarridx + 1], normals[normarridx + 2]);
                }
            }

            // compute face normals, if normals aren't provided
            if (normals.size() == 0)
            {
                auto v0 = _vertices[_indices[i * 3]].position;
                auto v1 = _vertices[_indices[i * 3 + 1]].position;
                auto v2 = _vertices[_indices[i * 3 + 2]].position;

                // counter-clockwise
                auto n = (v1 - v0).Cross(v2 - v0).Normalized();
                for (auto j : stdx::range(3u))
                    _vertices[_indices[i * 3 + j]].normal = n;
            }
        }
    }

    //if (translatetoorigin)
    //{
    //    geometry::aabb bounds;
    //    for (auto const& v : _vertices) bounds += v.position;

    //    // translate to (0,0,0)
    //    auto const& center = bounds.center();
    //    for (auto& v : _vertices) v.position -= center;
    //}
}

std::vector<instance_data> model::instancedata() const
{
    return { instance_data(matrix::Identity, globalresources::get().view()) };
}

}