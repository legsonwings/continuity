module;

#include "thirdparty/rapidobj.hpp"
#include "simplemath/simplemath.h"

module graphics:model;

import vec;
import :globalresources;
import engine;

namespace gfx
{

model::model(std::string const& objpath, resourcelist& transientres, modelloadparams loadparams)
{
    rapidobj::Result result = rapidobj::ParseFile(std::filesystem::path(objpath), rapidobj::MaterialLibrary::Default(rapidobj::Load::Optional));
    stdx::cassert(!result.error);

    rapidobj::Triangulate(result);
    stdx::cassert(!result.error);

    auto const& positions = result.attributes.positions;
    auto const& normals = result.attributes.normals;
    auto const& texcoords = result.attributes.texcoords;

    // make sure copying is safe
    static_assert(sizeof(std::decay_t<decltype(positions)>::value_type) * 3 == sizeof(stdx::vec3));
    static_assert(sizeof(std::decay_t<decltype(texcoords)>::value_type) * 2 == sizeof(stdx::vec2));

    stdx::vec3 const* const posstart = reinterpret_cast<stdx::vec3 const*>(positions.data());
    _vertices.positions = std::vector<stdx::vec3>(posstart, posstart + (positions.size() / 3));
    
    // flip uvs as directx uses top left as origin of uv space
    for (auto i = 0u; i < texcoords.size(); i += 2)
        _vertices.texcoords.push_back({ texcoords[i], 1.0f - texcoords[i + 1] });

    for (auto i = 0u; i < normals.size(); i += 3)
        _vertices.tbns.emplace_back().normal = stdx::vec3{ normals[i], normals[i + 1], normals[i + 2] };

    auto const numverts = positions.size() / 3;

    std::filesystem::path path = objpath;
    auto const modeldir = path.parent_path().string();

    auto fallbacktexcoord = int(_vertices.texcoords.size());
    _vertices.texcoords.emplace_back();

    auto createtexture = [&modeldir, &transientres](std::string filename, bool forcesrgb)
    {
        texture<accesstype::gpu> tex;
        std::filesystem::path filepath = filename;
        std::filesystem::path texturepath = globalresources::get().assetfullpath(modeldir + "/" + filepath.string());
        auto pathstr = texturepath.generic_string();

        if (!std::filesystem::exists(pathstr) || !std::filesystem::is_regular_file(pathstr))
            return tex;

        auto res = tex.createfromfile(pathstr, forcesrgb);
        transientres.insert(transientres.end(), res.begin(), res.end());
        return tex;
    };

    // todo : add a default material
    // add an empty material for faces without one
    auto fallbackmaterial = int(result.materials.size());
    result.materials.emplace_back();

    std::vector<uint32> materialidxtodescidx;
    for (auto const& m : result.materials)
    {
        material mat;
        if (!m.diffuse_texname.empty())
        {
            auto diffuse = createtexture(m.diffuse_texname, true);
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
            auto roughness = createtexture(roughnesstex, false);
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
            auto normal = createtexture(normaltexname, false);
            stdx::cassert(normal.valid());
            mat.normaltex = normal.createsrv().heapidx;
            _textures.push_back(normal);
        }

        stdx::cassert(m.metallic_texname.empty(), "Expected roughness and metallic to be be in same texture.");

        materialidxtodescidx.push_back(globalresources::get().addmat(mat));
    }

    uint32 const nummaxprims = uint32(numverts / 3);
    _materials.reserve(nummaxprims);

    for (auto const& shape : result.shapes)
    {
        _indices.reserve(_indices.size() + (shape.mesh.num_face_vertices.size() * 3));
        auto const& objindices = shape.mesh.indices;
        for (uint i(0); i < shape.mesh.num_face_vertices.size(); ++i)
        {
            stdx::cassert(shape.mesh.num_face_vertices[i] == 3);

            int old_normalindices[3] = {};
            rapidobj::Index face[3] = { objindices[i * 3], objindices[i * 3 + 1], objindices[i * 3 + 2] };

            stdx::vec3 ps[3];
            stdx::vec2 uvs[3];
            for (auto j : stdx::range(3u))
            {
                old_normalindices[j] = face[j].normal_index;
                if (face[j].normal_index == -1)
                {
                    // if the vertex doesn't have a normal then add it to the end
                    face[j].normal_index = int(_vertices.tbns.size());
                    
                    _vertices.tbns.emplace_back();
                }

                if (face[j].texcoord_index == -1) face[j].texcoord_index = fallbacktexcoord;

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
            stdx::vec3 t{}, b{};
            if (std::abs(det) > 1e-30f)
            {
                // create tbn based on uvs for consistency
                auto r = 1.0f / det;
                t = (e0 * y1 - e1 * y0) * r;
                b = (e1 * x0 - e0 * x1) * r;
            }

            // counter-clockwise
            auto n = e0.cross(e1).normalized();
            for (auto j : stdx::range(3u))
            {
                // compute vertex normals, if normals aren't provided
                if (old_normalindices[j] == -1) _vertices.tbns[face[j].normal_index].normal += n;

                _vertices.tbns[face[j].normal_index].tangent += t;
                _vertices.tbns[face[j].normal_index].bitangent += b;
            }

            int facematerial = shape.mesh.material_ids[i];
            facematerial = facematerial == -1 ? fallbackmaterial : facematerial;

            _materials.push_back(materialidxtodescidx[facematerial]);
        }
    }

    for (auto i : stdx::range(_vertices.tbns.size()))
    {
        auto& n = _vertices.tbns[i].normal;
        auto& t = _vertices.tbns[i].tangent;
        auto& b = _vertices.tbns[i].bitangent;

        n = n.normalized();
        t = (t - t.dot(n) * n).normalized();
        b = (b - b.dot(n) * n - b.dot(t) * t).normalized();
    }

    //if (loadparams.translatetoorigin)
    //{
    //    geometry::aabb bounds;
    //    for (auto const& v : _vertices) bounds += v.position;

    //    // translate to (0,0,0)
    //    auto const& center = bounds.center();
    //    for (auto& v : _vertices) v.position -= center;
    //}
}

model::model(std::vector<vertex> const& verts, std::vector<uint32> const& indices)
{
    for (auto& v : verts)
    {
        _vertices.positions.push_back({ v.position[0], v.position[1], v.position[2] });
        _vertices.texcoords.push_back({ v.texcoord[0], v.texcoord[1] });

        tbn tbn;
        tbn.normal = { v.normal[0], v.normal[1], v.normal[2] };
        tbn.tangent = { v.tangent[0], v.tangent[1], v.tangent[2] };
        tbn.bitangent = { v.bitangent[0], v.bitangent[1], v.bitangent[2] };

        _vertices.tbns.push_back(tbn);
    }

    // same index buffer for all attributes for now
    // todo : shapes should be updated to return vertices in new layout and this function can be updated after
    for (auto& i : indices)
        _indices.emplace_back(i, i, i);

    auto mat = globalresources::get().addmat(material{});
    for(auto i : stdx::range(verts.size() / 3))
        _materials.emplace_back(mat);
}

std::vector<instance_data> model::instancedata() const { return { instance_data{ matrix::Identity } }; }

}