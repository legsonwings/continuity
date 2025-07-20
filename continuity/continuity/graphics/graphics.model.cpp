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

    auto const numverts = positions.size() / 3;
     
    _vertices.resize(numverts);

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

    uint32 const numprims = uint32(numverts / 3);
    std::vector<uint32> primitivematerials;
    primitivematerials.reserve(numprims);
    for (auto const& shape : result.shapes)
    {
        auto const& objindices = shape.mesh.indices;
        for (uint i(0); i < shape.mesh.num_face_vertices.size(); ++i)
        {
            auto facematerial = shape.mesh.material_ids[i];
            primitivematerials.push_back(materialidxtodescidx[facematerial]);
            stdx::cassert(shape.mesh.num_face_vertices[i] == 3);

            for (auto j : stdx::range(3u))
            {
                auto vindex = objindices[i * 3 + j].position_index;
                _indices.push_back(vindex);

                // index into flat array of floats
                auto posarridx = vindex * 3;
                auto texcoordarridx = objindices[i * 3 + j].texcoord_index * 2;

                _vertices[vindex].position = vector3(positions[posarridx], positions[posarridx + 1], positions[posarridx + 2]);
                _vertices[vindex].texcoord = vector2(texcoords[texcoordarridx], texcoords[texcoordarridx + 1]);

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