module;

// include these first so min/max doesn't cause issues
#include "thirdparty/d3dx12.h"
#include "simplemath/simplemath.h"

#include <wrl.h>

module graphics;

import vec;
import engine;

// seems like the declarations is not available across files
using Microsoft::WRL::ComPtr;

namespace gfx
{

void setnamedebug(ComPtr<ID3D12Resource>& resource, std::string const& name)
{
#if _DEBUG
    resource->SetName(utils::strtowstr(name).c_str());
#endif
}

void setnamedebug(ComPtr<ID3D12Resource>& resource, std::wstring const& name)
{
#if _DEBUG
    resource->SetName(name.c_str());
#endif
}

shadertable::shadertable(uint recordsize, uint numrecords) : shaderrecordsize(recordsize), numshaderrecords(numrecords)
{
    d3dresource = gfx::create_uploadbuffer(&mapped_records, recordsize * numrecords);
}

ComPtr<ID3D12Resource> blas::kickoffbuild(D3D12_RAYTRACING_GEOMETRY_DESC const& geometrydesc)
{
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomlevelprebuildinfo = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomlevelinputs = {};

    bottomlevelinputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    bottomlevelinputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    bottomlevelinputs.NumDescs = 1;
    bottomlevelinputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bottomlevelinputs.pGeometryDescs = &geometrydesc;

    auto device = globalresources::get().device();
    auto cmdlist = globalresources::get().cmdlist();

    device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomlevelinputs, &bottomlevelprebuildinfo);
    stdx::cassert(bottomlevelprebuildinfo.ResultDataMaxSizeInBytes > 0);

    auto scratch = gfx::create_default_uavbuffer(bottomlevelprebuildinfo.ScratchDataSizeInBytes);
    d3dresource = gfx::create_accelerationstructbuffer(bottomlevelprebuildinfo.ResultDataMaxSizeInBytes);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomlevelbuilddesc = {};
    bottomlevelbuilddesc.Inputs = bottomlevelinputs;
    bottomlevelbuilddesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
    bottomlevelbuilddesc.DestAccelerationStructureData = gpuaddress();

    auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(d3dresource.Get());
    cmdlist->BuildRaytracingAccelerationStructure(&bottomlevelbuilddesc, 0, nullptr);
    cmdlist->ResourceBarrier(1, &barrier);

    return scratch;
}

std::array<ComPtr<ID3D12Resource>, tlas::numresourcetokeepalive> tlas::build(blasinstancedescs const& instancedescs)
{
    auto device = globalresources::get().device();
    auto cmdlist = globalresources::get().cmdlist();

    auto instancedescsresource = create_uploadbufferwithdata(instancedescs.descs.data(), uint(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instancedescs.descs.size()));

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC toplevelbuilddesc = {};
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& toplevelinputs = toplevelbuilddesc.Inputs;
    toplevelinputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    toplevelinputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    toplevelinputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    toplevelinputs.NumDescs = static_cast<UINT>(instancedescs.descs.size());
    toplevelinputs.InstanceDescs = instancedescsresource->GetGPUVirtualAddress();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO toplevelprebuildinfo = {};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&toplevelinputs, &toplevelprebuildinfo);
    stdx::cassert(toplevelprebuildinfo.ResultDataMaxSizeInBytes > 0);

    auto scratch = gfx::create_default_uavbuffer(toplevelprebuildinfo.ScratchDataSizeInBytes);
    d3dresource = gfx::create_accelerationstructbuffer(toplevelprebuildinfo.ResultDataMaxSizeInBytes);

    toplevelbuilddesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
    toplevelbuilddesc.DestAccelerationStructureData = gpuaddress();
    
    cmdlist->BuildRaytracingAccelerationStructure(&toplevelbuilddesc, 0, nullptr);
  
    return { scratch, instancedescsresource };
}

srv tlas::createsrv()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
    srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvdesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvdesc.RaytracingAccelerationStructure.Location = gpuaddress();
    
    // set resource as nullptr for acceleration structure srv's as resource is accessed using gpu address
    return globalresources::get().resourceheap().addsrv(srvdesc, nullptr);
}

std::array<ComPtr<ID3D12Resource>, triblas::numresourcetokeepalive> triblas::build(blasinstancedescs& instancedescs, geometryopacity opacity, rtvertexbuffer const& vertices, rtindexbuffer const& indices)
{
    D3D12_RAYTRACING_GEOMETRY_DESC geometrydesc = {};
    geometrydesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geometrydesc.Flags = opacity == geometryopacity::opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
    geometrydesc.Triangles.IndexBuffer = indices.gpuaddress();
    geometrydesc.Triangles.IndexCount = static_cast<UINT>(indices.numelements);
    geometrydesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geometrydesc.Triangles.Transform3x4 = 0;
    geometrydesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geometrydesc.Triangles.VertexCount = static_cast<UINT>(vertices.numelements);
    geometrydesc.Triangles.VertexBuffer.StartAddress = vertices.gpuaddress();
    geometrydesc.Triangles.VertexBuffer.StrideInBytes = sizeof(stdx::vec3);

    auto scratch = kickoffbuild(geometrydesc);

    uint const instanceidx = instancedescs.descs.size();
    D3D12_RAYTRACING_INSTANCE_DESC& instancedesc = instancedescs.descs.emplace_back();
    instancedesc = {};
    instancedesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
    instancedesc.Transform[0][0] = instancedesc.Transform[1][1] = instancedesc.Transform[2][2] = 1;
    instancedesc.InstanceMask = 1;
    instancedesc.AccelerationStructure = gpuaddress();
    instancedesc.InstanceContributionToHitGroupIndex = instanceidx;

    return { scratch };
}

std::array<ComPtr<ID3D12Resource>, proceduralblas::numresourcetokeepalive> proceduralblas::build(blasinstancedescs& instancedescs, geometryopacity opacity, geometry::aabb const& aabb)
{
    D3D12_RAYTRACING_AABB rtaabb;
    rtaabb.MinX = aabb.min_pt[0];
    rtaabb.MinY = aabb.min_pt[1];
    rtaabb.MinZ = aabb.min_pt[2];
    rtaabb.MaxX = aabb.max_pt[0];
    rtaabb.MaxY = aabb.max_pt[1];
    rtaabb.MaxZ = aabb.max_pt[2];

    auto aabbbuffer = create_uploadbufferwithdata(&rtaabb, sizeof(rtaabb));

	D3D12_RAYTRACING_GEOMETRY_DESC geometrydesc = {};
	geometrydesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS;
	geometrydesc.AABBs.AABBCount = 1;
	geometrydesc.Flags = opacity == geometryopacity::opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
    geometrydesc.AABBs.AABBs.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB);
    geometrydesc.AABBs.AABBs.StartAddress = aabbbuffer->GetGPUVirtualAddress();

    auto scratch = kickoffbuild(geometrydesc);

    uint const instanceidx = instancedescs.descs.size();
    D3D12_RAYTRACING_INSTANCE_DESC& instancedesc = instancedescs.descs.emplace_back();
    instancedesc = {};
    instancedesc.Transform[0][0] = instancedesc.Transform[1][1] = instancedesc.Transform[2][2] = 1;
    instancedesc.InstanceMask = 1;
    instancedesc.AccelerationStructure = gpuaddress();
    instancedesc.InstanceContributionToHitGroupIndex = instanceidx;

    return { aabbbuffer, scratch };
}

void raytrace::dispatchrays(shadertable const& raygen, shadertable const& miss, shadertable const& hitgroup, ID3D12StateObject* stateobject, uint width, uint height)
{
    // this function expects resources and state to be setup
    D3D12_DISPATCH_RAYS_DESC dispatchdesc = {};

    dispatchdesc.HitGroupTable.StartAddress = hitgroup.gpuaddress();
    dispatchdesc.HitGroupTable.SizeInBytes = hitgroup.d3dresource->GetDesc().Width;
    dispatchdesc.HitGroupTable.StrideInBytes = UINT(hitgroup.recordsize());
    dispatchdesc.MissShaderTable.StartAddress = miss.gpuaddress();
    dispatchdesc.MissShaderTable.SizeInBytes = miss.d3dresource->GetDesc().Width;
    dispatchdesc.MissShaderTable.StrideInBytes = UINT(miss.recordsize());
    dispatchdesc.RayGenerationShaderRecord.StartAddress = raygen.gpuaddress();
    dispatchdesc.RayGenerationShaderRecord.SizeInBytes = raygen.d3dresource->GetDesc().Width;
    dispatchdesc.Width = UINT(width);
    dispatchdesc.Height = UINT(height);
    dispatchdesc.Depth = 1;

    globalresources::get().cmdlist()->DispatchRays(&dispatchdesc);
}

void raytrace::copyoutputtorendertarget(rtouttexture const& rtoutput)
{
    auto& globalres = globalresources::get();

    auto cmd_list = globalres.cmdlist();
    auto* rendertarget = globalres.rendertarget().Get();

    D3D12_RESOURCE_BARRIER precopybarriers[2];
    precopybarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(rendertarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    precopybarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(rtoutput.d3dresource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    cmd_list->ResourceBarrier(ARRAYSIZE(precopybarriers), precopybarriers);

    cmd_list->CopyResource(rendertarget, rtoutput.d3dresource.Get());

    D3D12_RESOURCE_BARRIER postcopybarriers[2];
    postcopybarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(rendertarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    postcopybarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(rtoutput.d3dresource.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    cmd_list->ResourceBarrier(ARRAYSIZE(postcopybarriers), postcopybarriers);
}

//std::string generaterandom_matcolor(stdx::ext<material, bool> definition, std::optional<std::string> const& preferred_name)
//{
//    std::random_device rd;
//    static std::mt19937 re(rd());
//    static constexpr uint matgenlimit = 10000u;
//    static std::uniform_int_distribution<uint> matnumberdist(0, matgenlimit);
//    static std::uniform_real_distribution<float> colordist(0.f, 1.f);
//
//    stdx::vec3 const color = { colordist(re), colordist(re), colordist(re) };
//    std::string basename("mat");
//
//    std::string matname = (preferred_name.has_value() ? preferred_name.value() : basename) + std::to_string(matnumberdist(re));
//
//    while (globalresources::get().matmap().find(matname) != globalresources::get().matmap().end()) { matname = basename + std::to_string(matnumberdist(re)); }
//
//    definition->basecolour = stdx::vec4{ color[0], color[1], color[2], definition->basecolour[3]};
//    globalresources::get().addmat(matname, definition, definition.ex());
//    return matname;
//}

}