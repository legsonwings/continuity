module;

#include "simplemath/simplemath.h"
#include <d3d12.h>
#include "thirdparty/d3dx12.h"
#include "thirdparty/dxhelpers.h"

module raytrace;

import stdx;
import vec;
import std;

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

using namespace DirectX;

// todo : remove these
using vector3 = DirectX::SimpleMath::Vector3;
using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;

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
    using geometry::cube;
    using geometry::sphere;
    using gfx::bodyparams;
    using gfx::material;

    auto& globalres = gfx::globalresources::get();
    auto& globals = globalres.cbuffer().data();

    // initialize lights
    globals.numdirlights = 1;
    globals.numpointlights = 0;

    globals.ambient = { 0.1f, 0.1f, 0.1f, 1.0f };
    globals.lights[0].direction = vector3{ 0.3f, -0.27f, 0.57735f }.Normalized();
    globals.lights[0].color = { 0.2f, 0.2f, 0.2f };

    //globals.viewproj = (globalres.get().view().view * globalres.get().view().proj).Invert().Transpose();
    //globalres.cbuffer().updateresource();

    constantbuffer.createresource();

    gfx::resourcelist res;

    {
        gfx::raytraceshaders rtshaders;
        rtshaders.raygen = raygenShaderName;
        rtshaders.miss = missShaderName;
        rtshaders.tri_hitgrp.name = hitGroupName;
        rtshaders.tri_hitgrp.closesthit = closestHitShaderName;

        auto& raytraceipelinepipeline_objs = globalres.addraytracingpso("raytrace", "raytrace_rs.cso", rtshaders);

        auto& device = globalres.device();
        auto& cmdlist = globalres.cmdlist();

        gfx::model model("models/spot.obj");
        gfx::geometryopacity const opacity = gfx::geometryopacity::opaque;
        gfx::blasinstancedescs instancedescs;

        // cannot use stdx::join because ComPtr is too smart for its own good
        for (auto r : triblas.build(instancedescs, opacity, model.vertices, model.indices))
            res.push_back(r);

        for (auto r : tlas.build(instancedescs))
            res.push_back(r);

        ComPtr<ID3D12StateObjectProperties> stateobjectproperties;
        ThrowIfFailed(raytraceipelinepipeline_objs.pso_raytracing.As(&stateobjectproperties));
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
        // 0. const buffer for frame0 
        // 1. const buffer for frame1
        // 2. tlas
        // 3. ray trace output
        constantbuffer.createcbv();
        tlas.createsrv();
        raytracingoutput.createuav();
    }

    return res;
}

void raytrace::render(float dt)
{
    auto& globalres = gfx::globalresources::get();
    auto& framecbuffer = constantbuffer.data(globalres.frameindex());

    framecbuffer.campos = camera.GetCurrentPosition();
    framecbuffer.inv_viewproj = utils::to_matrix4x4((globalres.view().view * globalres.view().proj).Invert());

    auto cmd_list = globalres.cmdlist();
    auto const& pipelineobjects = globalres.psomap().find("raytrace")->second;

    // bind the global root signature, heaps and frame index, other resources are bindless 
    cmd_list->SetDescriptorHeaps(1, globalres.resourceheap().d3dheap.GetAddressOf());
    cmd_list->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    cmd_list->SetComputeRoot32BitConstant(0, UINT(globalres.frameindex()), 0);
    cmd_list->SetPipelineState1(pipelineobjects.pso_raytracing.Get());

    gfx::raytrace rt;
    rt.dispatchrays(raygenshadertable, missshadertable, hitgroupshadertable, pipelineobjects.pso_raytracing.Get(), 720, 720);
    rt.copyoutputtorendertarget(raytracingoutput);
}