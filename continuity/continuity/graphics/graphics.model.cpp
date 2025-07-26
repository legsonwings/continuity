module;

#include "thirdparty/rapidobj.hpp"
#include "simplemath/simplemath.h"

module graphics:model;

import vec;
import :globalresources;
import engine;

namespace gfx
{

model::model(std::string const& objpath, modelloadparams loadparams)
{
    rapidobj::Result result = rapidobj::ParseFile(std::filesystem::path(objpath), rapidobj::MaterialLibrary::Default(rapidobj::Load::Optional));
    stdx::cassert(!result.error);

    rapidobj::Triangulate(result);
    stdx::cassert(!result.error);

    auto const& positions = result.attributes.positions;
    auto const& normals = result.attributes.normals;
    auto const& texcoords = result.attributes.texcoords;

    // make sure copying is safe
    static_assert(sizeof(std::decay_t<decltype(positions)>::value_type) * 3 == sizeof(vector3));
    static_assert(sizeof(std::decay_t<decltype(texcoords)>::value_type) * 2 == sizeof(vector2));

    vector3 const* const posstart = reinterpret_cast<vector3 const*>(positions.data());
    _vertices.positions = std::vector<vector3>(posstart, posstart + (positions.size() / 3));
    
    vector2 const* const texcoordsstart = reinterpret_cast<vector2 const*>(texcoords.data());
    _vertices.texcoords = std::vector<vector2>(texcoordsstart, texcoordsstart + (texcoords.size() / 2));

    for (auto i = 0u; i < normals.size(); i += 3)
        _vertices.tbns.emplace_back().normal = vector3{ normals[i], normals[i + 1], normals[i + 2] };

    auto const numverts = positions.size() / 3;

    std::filesystem::path path = objpath;
    auto const modeldir = path.parent_path().string();

    auto createtexture = [&modeldir](std::string filename)
    {
        texture<accesstype::gpu> tex;
        std::filesystem::path filepath = filename;
        std::filesystem::path texturepath = globalresources::get().assetfullpath(modeldir + "/" + filepath.string());
        auto pathstr = texturepath.generic_string();

        if (!std::filesystem::exists(pathstr) || !std::filesystem::is_regular_file(pathstr))
            return tex;

        tex.createfromfile(pathstr);
        return tex;
    };

    std::vector<uint32> materialidxtodescidx;
    for (auto const& m : result.materials)
    {
        material mat;
        if (!m.diffuse_texname.empty())
        {
            auto diffuse = createtexture(m.diffuse_texname);
            stdx::cassert(diffuse.valid());
            mat.diffusetex = diffuse.createsrv().heapidx;
            _textures.push_back(diffuse);
        }
        else
        {
            // expect same single channel transmittance
            mat.basecolour = stdx::vec4{ m.diffuse[0], m.diffuse[1], m.diffuse[2], (1.0f - m.transmittance[0]) };
        }

        std::string roughnesstex = m.roughness_texname;

        // if roughness tex is not specified then specular slot may be used for the roughness texture
        // todo : this is temp, should actually fix the materials in source files
        if (roughnesstex.empty() && loadparams.specularasmetallicroughness)
        {
            // there is an .mtl extension for pbr textures however some models do not use it and resuse legacy texture slots
            // this is acutally a metallic roughness texture using b and g channels respectively
            roughnesstex = m.specular_texname;
        }

        if (!roughnesstex.empty())
        {
            auto roughness = createtexture(roughnesstex);
            stdx::cassert(roughness.valid());
            mat.roughnesstex = roughness.createsrv().heapidx;
            _textures.push_back(roughness);
        }
        else
        {
            mat.roughness = 1.0f - sqrt(m.shininess / 1000.0f);
            stdx::vec3 spec{ m.specular[0], m.specular[1], m.specular[2] };
            mat.metallic = spec.length() > 0.08f ? 1.0f : 0.0f;
        }

        std::string normaltexname = m.normal_texname;

        // if normal tex is not specified then ambient slot may be used for the roughness texture
        // todo : this is temp, should actually fix the materials in source files
        if (normaltexname.empty() && loadparams.ambientasnormal)
        {
            // there is an .mtl extension for pbr textures however some models do not use it and resuse legacy texture slots
            normaltexname = m.ambient_texname;
        }

        if (!normaltexname.empty())
        {
            auto normal = createtexture(normaltexname);
            stdx::cassert(normal.valid());
            mat.normaltex = normal.createsrv().heapidx;
            _textures.push_back(normal);
        }

        stdx::cassert(m.metallic_texname.empty(), "Expected roughness and metallic to be be in same texture.");

        materialidxtodescidx.push_back(globalresources::get().addmat(mat));
    }

    uint32 const nummaxprims = uint32(numverts / 3);
    std::vector<uint32> primitivematerials;
    primitivematerials.reserve(nummaxprims);

    for (auto const& shape : result.shapes)
    {
        _indices.reserve(_indices.size() + (shape.mesh.num_face_vertices.size() * 3));
        auto const& objindices = shape.mesh.indices;
        for (uint i(0); i < shape.mesh.num_face_vertices.size(); ++i)
        {
            stdx::cassert(shape.mesh.num_face_vertices[i] == 3);

            rapidobj::Index face[3] = { objindices[i * 3], objindices[i * 3 + 1], objindices[i * 3 + 2] };

            vector3 ps[3];
            vector2 uvs[3];
            for (auto j : stdx::range(3u))
            {
                // all vectors in tbn are similarly indexed, so use normal index
                _indices.emplace_back(face[j].position_index, face[j].texcoord_index, face[j].normal_index);

                ps[j] = _vertices.positions[face[j].position_index];
                uvs[j] = _vertices.texcoords[face[j].texcoord_index];
            }

            auto x0 = uvs[1][0] - uvs[0][0];
            auto x1 = uvs[2][0] - uvs[0][0];
            auto y0 = uvs[1][1] - uvs[0][1];
            auto y1 = uvs[2][1] - uvs[0][1];

            auto det = x0 * y1 - x1 * y0;

            auto e0 = ps[1] - ps[0];
            auto e1 = ps[2] - ps[0];

            // there are lot of cases where triangles are degenerates in uv space but not in R3
            // caused by same texture coord used for two vertices, but position is different(where is this coming from?)
            // avoid such cases by setting tangent and bitangent to zero to prevent nan propagation
            vector3 t, b;
            if (std::abs(det) > 1e-37f)
            {
                // create tbn based on uvs for consistency
                auto r = 1.0f / det;
                t = (e0 * y1 - e1 * y0) * r;
                b = (e1 * x0 - e0 * x1) * r;
            }

            // counter-clockwise
            auto n = e0.Cross(e1).Normalized();
            for (auto j : stdx::range(3u))
            {
                // compute vertex normals, if normals aren't provided
                if (normals.size() == 0) _vertices.tbns[face[j].normal_index].normal += n;

                _vertices.tbns[face[j].normal_index].tangent += t;
                _vertices.tbns[face[j].normal_index].bitangent += b;
            }

            auto facematerial = shape.mesh.material_ids[i];
            primitivematerials.push_back(materialidxtodescidx[facematerial]);
        }
    }

    for (auto i : stdx::range(_vertices.tbns.size()))
    {
        auto& n = _vertices.tbns[i].normal;
        auto& t = _vertices.tbns[i].tangent;
        auto& b = _vertices.tbns[i].bitangent;

        n.Normalize();
        t = (t - t.Dot(n) * n).Normalized();
        b = (b - b.Dot(n) * n - b.Dot(t) * t).Normalized();
    }

    _primitivematerials.create(primitivematerials);
    _primmaterialsdescidx = _primitivematerials.createsrv().heapidx;

    //if (loadparams.translatetoorigin)
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
    instance_data data(matrix::Identity, globalresources::get().view());
    data.primmaterialsidx = _primmaterialsdescidx;
    return { data };
}

}