module;

#include "simplemath/simplemath.h"
#include "thirdparty/d3dx12.h"
#include "thirdparty/dxhelpers.h"

module raytrace;

import stdx;
import vec;
import std;
import activesample;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::raytrace>(view_data const& data)
{
	return std::move(std::make_unique<raytrace>(data));
}

}

#define namkaran(d3dobject) { d3dobject->SetName(utils::strtowstr(#d3dobject).c_str()); }
#define namkaranres(gfxresource) { gfxresource.d3dresource->SetName(utils::strtowstr(#gfxresource).c_str()); }

// raytracing stuff
static constexpr char const* hitgroupname = "trianglehitgroup";
static constexpr char const* raygenshadername = "raygenshader";
static constexpr char const* closesthitshadername = "closesthitshader_triangle";
static constexpr char const* missshadername = "missshader";

raytrace::raytrace(view_data const& viewdata) : sample_base(viewdata)
{
	camera.Init({ 0.f, 0.f, -30.f });
	camera.SetMoveSpeed(10.0f);
}

gfx::resourcelist raytrace::create_resources(gfx::renderer& renderer)
{
    auto& globalres = gfx::globalresources::get();

    //constantbuffer.createresource();

    gfx::resourcelist res;

    {
        gfx::raytraceshaders rtshaders;
        rtshaders.raygen = raygenshadername;
        rtshaders.miss = missshadername;
        rtshaders.tri_hitgrp.name = hitgroupname;
        rtshaders.tri_hitgrp.closesthit = closesthitshadername;

        auto& raytracepipeline_objs = globalres.addraytracingpso("raytrace", "raytrace_rs.cso", rtshaders, 56, 2 * 4);

        gfx::model model("models/spot.obj", res);
        gfx::geometryopacity const opacity = gfx::geometryopacity::opaque;
        gfx::blasinstancedescs instancedescs;

        // one material id per blas
        std::vector<uint32> material_idsdata(1u, 0u);
        std::vector<gfx::material> materials_data(1u);
        materials_data[0].basecolour = { 0.0f, 1.0f, 0.0f, 1.0f };
        materials_data[0].metallic = 1u;
        materials_data[0].roughness = 0.25f;
        materials_data[0].reflectance = 0.5f;

        // todo 
        //res.push_back(vertexbuffer.create(model.vertices));
        //res.push_back(indexbuffer.create(model.indices));
        //res.push_back(materialids.create(material_idsdata));
        //res.push_back(materials.create(materials_data));

        // todo vertices now has normal too 
        //vertexbuffer.create(model.vertices);
        //indexbuffer.create(model.indices);
        //materialids.create(material_idsdata);
        //materials.create(materials_data);

        // cannot use stdx::join because ComPtr is too smart for its own good
        for (auto r : triblas.build(instancedescs, opacity, vertexbuffer, indexbuffer))
            res.push_back(r);

        for (auto r : tlas.build(instancedescs))
            res.push_back(r);

        ComPtr<ID3D12StateObjectProperties> stateobjectproperties;
        ThrowIfFailed(raytracepipeline_objs.pso_raytracing.As(&stateobjectproperties));
        auto* stateobjectprops = stateobjectproperties.Get();

        // get shader id's
        void* raygenshaderid = stateobjectprops->GetShaderIdentifier(utils::strtowstr(raygenshadername).c_str());
        void* missshaderid = stateobjectprops->GetShaderIdentifier(utils::strtowstr(missshadername).c_str());
        void* hitgroupshaderids_trianglegeometry = stateobjectprops->GetShaderIdentifier(utils::strtowstr(hitgroupname).c_str());

        // now build shader tables
        // only one records for ray gen and miss shader tables
        raygenshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
        raygenshadertable.addrecord(raygenshaderid);

        missshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
        missshadertable.addrecord(missshaderid);

        // triangle record for hitgroup shader table
        hitgroupshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
        hitgroupshadertable.addrecord(hitgroupshaderids_trianglegeometry);

        raytracingoutput.create(DXGI_FORMAT_R8G8B8A8_UNORM, stdx::vecui2{ viewdata.width, viewdata.height }, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // todo : these are incorrect now
        // we don't store descriptors, as the indices are hardcoded in shaders
        // 0 rt srv
        // 1 rt uav
        // 2 const buffer 
        // 3 tlas
        // 4 ray trace output
        // 5 vertex buffers
        // 6 index buffers
        // 7 material ids
        // 8 material data
 
        // todo : bindless isn't hardcorded anymore
        //constantbuffer.createcbv();
        //tlas.createsrv();
        //raytracingoutput.createuav();
        //vertexbuffer.createsrv();
        //indexbuffer.createsrv();
        //materialids.createsrv();
        //materials.createsrv();
    }

    return res;
}

void raytrace::render(float dt, gfx::renderer& renderer)
{
    auto& globalres = gfx::globalresources::get();
    //auto& framecbuffer = constantbuffer.data(0);

    //framecbuffer.sundir = stdx::vec3{ 1.0f, 0.0f, 0.0f }.normalized();
    //framecbuffer.campos = camera.GetCurrentPosition();
    //framecbuffer.inv_viewproj = utils::to_matrix4x4((globalres.view().view * globalres.view().proj).Invert());

    auto& cmdlist = renderer.deviceres().cmdlist;
    auto const& pipelineobjects = globalres.psomap().find("raytrace")->second;

    // bind the global root signature, heaps and frame index, other resources are bindless 
    cmdlist->SetDescriptorHeaps(1, globalres.resourceheap().d3dheap.GetAddressOf());
    cmdlist->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    cmdlist->SetPipelineState1(pipelineobjects.pso_raytracing.Get());

    gfx::raytrace rt;
    rt.dispatchrays(raygenshadertable, missshadertable, hitgroupshadertable, pipelineobjects.pso_raytracing.Get(), 720, 720);
    rt.copyoutputtorendertarget(cmdlist.Get(), raytracingoutput, renderer.rendertarget->d3dresource.Get());
}