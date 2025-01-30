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

static constexpr float roomextents = 1.6f;

// raytracing stuff
static constexpr char const* hitGroupName = "MyHitGroup";
static constexpr char const* raygenShaderName = "MyRaygenShader";
static constexpr char const* closestHitShaderName = "MyClosestHitShader_Triangle";
static constexpr char const* missShaderName = "MyMissShader";

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
    globals.numpointlights = 2;

    globals.ambient = { 0.1f, 0.1f, 0.1f, 1.0f };
    globals.lights[0].direction = vector3{ 0.3f, -0.27f, 0.57735f }.Normalized();
    globals.lights[0].color = { 0.2f, 0.2f, 0.2f };

    globals.lights[1].position = { -15.f, 15.f, -15.f };
    globals.lights[1].color = { 1.f, 1.f, 1.f };
    globals.lights[1].range = 40.f;

    globals.lights[2].position = { 15.f, 15.f, -15.f };
    globals.lights[2].color = { 1.f, 1.f, 1.f };
    globals.lights[2].range = 40.f;

    globals.viewproj = (globalres.get().view().view * globalres.get().view().proj).Invert().Transpose();
    globalres.cbuffer().updateresource();

    // since these use static vertex buffers, just send 0 as maxverts
    boxes.emplace_back(cube{ vector3{0.f, 0.f, 0.f}, vector3{ roomextents } }, &cube::vertices_flipped, &cube::instancedata, bodyparams{ 0, 1, "instanced" });

    gfx::resourcelist res;
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes)) { stdx::append(b->create_resources(), res); };

    // rt triangle
    {
        gfx::raytraceshaders rtshaders;
        rtshaders.raygen = raygenShaderName;
        rtshaders.miss = missShaderName;
        rtshaders.tri_hitgrp.name = hitGroupName;
        rtshaders.tri_hitgrp.closesthit = closestHitShaderName;

        auto& raytraceipelinepipeline_objs = globalres.addraytracingpso("raytrace", "raytrace_rs.cso", rtshaders);

        auto& device = globalres.device();
        auto& cmdlist = globalres.cmdlist();

        gfx::model model("spot.obj");
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

        // Create the output resource. The dimensions and format should match the swap-chain.
        auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 720, 720, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&raytracingoutput)));
        namkaran(raytracingoutput);

        auto descriptorHeapCpuBase = globalres.srvheap()->GetCPUDescriptorHandleForHeapStart();

        D3D12_CPU_DESCRIPTOR_HANDLE cpudescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, 0, UINT(gfx::srvcbvuav_descincrementsize()));
        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(raytracingoutput.Get(), nullptr, &UAVDesc, cpudescriptor);
        raytracingoutput_uavgpudescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(globalres.srvheap()->GetGPUDescriptorHandleForHeapStart(), 0, UINT(gfx::srvcbvuav_descincrementsize()));
    }

    return res;
}

void raytrace::update(float dt)
{
    sample_base::update(dt);
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes)) b->update(dt);
}

void raytrace::render(float dt)
{
    auto& globalres = gfx::globalresources::get();
    auto& globals = globalres.cbuffer().data();

    // hack : should use different const buffer
    globals.viewproj = (globalres.get().view().view * globalres.get().view().proj).Invert().Transpose();
    globalres.cbuffer().updateresource();

    // render the room inner walls
    //for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes)) b->render(dt, { false });

    auto cmd_list = globalres.cmdlist();

    auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
    {
        // Since each shader table has only one shader record, the stride is same as the size.
        dispatchDesc->HitGroupTable.StartAddress = hitgroupshadertable.d3dresource->GetGPUVirtualAddress();
        dispatchDesc->HitGroupTable.SizeInBytes = hitgroupshadertable.d3dresource->GetDesc().Width;
        dispatchDesc->HitGroupTable.StrideInBytes = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;    // no local root arguments
        dispatchDesc->MissShaderTable.StartAddress = missshadertable.d3dresource->GetGPUVirtualAddress();
        dispatchDesc->MissShaderTable.SizeInBytes = missshadertable.d3dresource->GetDesc().Width;
        dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
        dispatchDesc->RayGenerationShaderRecord.StartAddress = raygenshadertable.d3dresource->GetGPUVirtualAddress();
        dispatchDesc->RayGenerationShaderRecord.SizeInBytes = raygenshadertable.d3dresource->GetDesc().Width;
        dispatchDesc->Width = 720;
        dispatchDesc->Height = 720;
        dispatchDesc->Depth = 1;
        commandList->SetPipelineState1(stateObject);
        commandList->DispatchRays(dispatchDesc);
    };

    auto const& pipelineobjects = globalres.psomap().find("raytrace")->second;

    cmd_list->SetComputeRootSignature(pipelineobjects.root_signature.Get());

    // Bind the heaps, acceleration structure and dispatch rays.    
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    cmd_list->SetDescriptorHeaps(1, globalres.srvheap().GetAddressOf());
    cmd_list->SetComputeRootDescriptorTable(0, raytracingoutput_uavgpudescriptor);
    cmd_list->SetComputeRootShaderResourceView(1, tlas.d3dresource->GetGPUVirtualAddress());
    cmd_list->SetComputeRootConstantBufferView(2, globalres.cbuffer().gpuaddress());
    DispatchRays(cmd_list.Get(), pipelineobjects.pso_raytracing.Get(), &dispatchDesc);

    auto* rendertarget = globalres.rendertarget().Get();

    D3D12_RESOURCE_BARRIER preCopyBarriers[2];
    preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(rendertarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracingoutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    cmd_list->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

    cmd_list->CopyResource(rendertarget, raytracingoutput.Get());
    cmd_list->SetPipelineState1(pipelineobjects.pso_raytracing.Get());

    D3D12_RESOURCE_BARRIER postCopyBarriers[2];
    postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(rendertarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracingoutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    gfx::raytrace rt;
    rt.dispatchrays(raygenshadertable, missshadertable, hitgroupshadertable, pipelineobjects.pso_raytracing.Get(), 720, 720);
    rt.copyoutputtorendertarget(raytracingoutput.Get());

    cmd_list->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}