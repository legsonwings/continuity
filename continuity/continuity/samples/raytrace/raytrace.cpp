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
static constexpr char const* hitGroupName = "trianglehitgroup";
static constexpr char const* raygenShaderName = "raygenshader";
static constexpr char const* closestHitShaderName = "closesthitshader_triangle";
static constexpr char const* missShaderName = "missshader";

raytrace::raytrace(view_data const& viewdata) : sample_base(viewdata)
{
	camera.Init({ 0.f, 0.f, -30.f });
	camera.SetMoveSpeed(10.0f);
}

gfx::resourcelist raytrace::create_resources()
{
    auto& globalres = gfx::globalresources::get();

    constantbuffer.createresource();

    gfx::resourcelist res;

    {
        gfx::raytraceshaders rtshaders;
        rtshaders.raygen = raygenShaderName;
        rtshaders.miss = missShaderName;
        rtshaders.tri_hitgrp.name = hitGroupName;
        rtshaders.tri_hitgrp.closesthit = closestHitShaderName;

        auto& raytracepipeline_objs = globalres.addraytracingpso("raytrace", "raytrace_rs.cso", rtshaders);

        auto& device = globalres.device();
        auto& cmdlist = globalres.cmdlist();

        gfx::model model("models/spot.obj");
        gfx::geometryopacity const opacity = gfx::geometryopacity::opaque;
        gfx::blasinstancedescs instancedescs;

        // one material id per blas
        std::vector<uint32> material_idsdata(1u, 0u);
        std::vector<rt::material> materials_data(1u);

        materials_data[0].colour[0] = 0.0f;
        materials_data[0].colour[1] = 1.0f;
        materials_data[0].colour[2] = 0.0f;
        materials_data[0].colour[3] = 1.0f;
        materials_data[0].metallic = 1u;
        materials_data[0].roughness = 0.25f;
        materials_data[0].reflectance = 0.5f;

        res.push_back(vertexbuffer.create(model.vertices));
        res.push_back(indexbuffer.create(model.indices));
        res.push_back(materialids.create(material_idsdata));
        res.push_back(materials.create(materials_data));

        // cannot use stdx::join because ComPtr is too smart for its own good
        for (auto r : triblas.build(instancedescs, opacity, vertexbuffer, indexbuffer))
            res.push_back(r);

        for (auto r : tlas.build(instancedescs))
            res.push_back(r);

        ComPtr<ID3D12StateObjectProperties> stateobjectproperties;
        ThrowIfFailed(raytracepipeline_objs.pso_raytracing.As(&stateobjectproperties));
        auto* stateobjectprops = stateobjectproperties.Get();

        // get shader id's
        void* raygenshaderid = stateobjectprops->GetShaderIdentifier(utils::strtowstr(raygenShaderName).c_str());
        void* missshaderid = stateobjectprops->GetShaderIdentifier(utils::strtowstr(missShaderName).c_str());
        void* hitgroupshaderids_trianglegeometry = stateobjectprops->GetShaderIdentifier(utils::strtowstr(hitGroupName).c_str());

        // now build shader tables
        // only one records for ray gen and miss shader tables
        raygenshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
        raygenshadertable.addrecord(raygenshaderid);

        missshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
        missshadertable.addrecord(missshaderid);

        // triangle and procedural aabb records for hitgroup shader table
        hitgroupshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
        hitgroupshadertable.addrecord(hitgroupshaderids_trianglegeometry);

        raytracingoutput = gfx::texture(DXGI_FORMAT_R8G8B8A8_UNORM, stdx::vecui2{ 720, 720 }, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        
        // we don't store descriptors, as the indices are hardcoded in shaders
        // 0 const buffer
        // 1 tlas
        // 2 ray trace output
        // 3 vertex buffers
        // 4 index buffers
        // 5 material ids
        // 6 material data
        constantbuffer.createcbv();
        tlas.createsrv();
        raytracingoutput.createuav();
        vertexbuffer.createsrv();
        indexbuffer.createsrv();
        materialids.createsrv();
        materials.createsrv();
    }

    return res;
}

void raytrace::render(float dt)
{
    auto& globalres = gfx::globalresources::get();
    auto& framecbuffer = constantbuffer.data(0);

    framecbuffer.sundir = stdx::vec3{ 1.0f, 0.0f, 0.0f }.normalized();
    framecbuffer.campos = camera.GetCurrentPosition();
    framecbuffer.inv_viewproj = utils::to_matrix4x4((globalres.view().view * globalres.view().proj).Invert());

    auto cmd_list = globalres.cmdlist();
    auto const& pipelineobjects = globalres.psomap().find("raytrace")->second;

    // bind the global root signature, heaps and frame index, other resources are bindless 
    cmd_list->SetDescriptorHeaps(1, globalres.resourceheap().d3dheap.GetAddressOf());
    cmd_list->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    cmd_list->SetPipelineState1(pipelineobjects.pso_raytracing.Get());

    gfx::raytrace rt;
    rt.dispatchrays(raygenshadertable, missshadertable, hitgroupshadertable, pipelineobjects.pso_raytracing.Get(), 720, 720);
    rt.copyoutputtorendertarget(raytracingoutput);
}