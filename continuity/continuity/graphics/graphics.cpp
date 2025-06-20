module;

// include these first so min/max doesn't cause issues
#include "thirdparty/rapidobj.hpp"
#include "thirdparty/dxhelpers.h"
#include "thirdparty/d3dx12.h"

#include <wrl.h>
#include <d3d12.h>

module graphics;

import vec;
import engine;

// seems like the declarations is not available across files
using Microsoft::WRL::ComPtr;

alignedlinearallocator::alignedlinearallocator(uint alignment) : _alignment(alignment)
{
	std::size_t sz = buffersize;
    stdx::cassert([&] { return stdx::ispowtwo(_alignment); });
	void* ptr = _buffer;
	_currentpos = reinterpret_cast<std::byte*>(std::align(_alignment, buffersize - alignment, ptr, sz));
}

bool alignedlinearallocator::canallocate(uint size) const { return ((_currentpos - &_buffer[0]) + size) < buffersize; }

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

void raytrace::copyoutputtorendertarget(texture const& rtoutput)
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

srv resourceheap::addsrv(D3D12_SHADER_RESOURCE_VIEW_DESC viewdesc, ID3D12Resource* res)
{
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(d3dheap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(currslot * srvcbvuav_descincrementsize()));
    device->CreateShaderResourceView(res, &viewdesc, deschandle);
    return { currslot++, viewdesc };
}

uav resourceheap::adduav(D3D12_UNORDERED_ACCESS_VIEW_DESC viewdesc, ID3D12Resource* res)
{
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(d3dheap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(currslot * srvcbvuav_descincrementsize()));
    device->CreateUnorderedAccessView(res, nullptr, &viewdesc, deschandle);
    return { currslot++, viewdesc };
}

cbv resourceheap::addcbv(D3D12_CONSTANT_BUFFER_VIEW_DESC viewdesc, ID3D12Resource* res)
{
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(d3dheap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(currslot * srvcbvuav_descincrementsize()));
    device->CreateConstantBufferView(&viewdesc, deschandle);
    return { currslot++, viewdesc };
}

D3D12_GPU_DESCRIPTOR_HANDLE resourceheap::gpudeschandle(uint slot) const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(d3dheap->GetGPUDescriptorHandleForHeapStart(), 0, UINT(slot * gfx::srvcbvuav_descincrementsize()));
}

texture::texture(DXGI_FORMAT dxgiformat, stdx::vecui2 dimensions, D3D12_RESOURCE_STATES state) : format(dxgiformat), dims(dimensions)
{
    d3dresource = createtexture_default(dims[0], dims[1], format, state);
}

srv texture::createsrv() const
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
    srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvdesc.Format = format;
    srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvdesc.Texture2D.MipLevels = 1;

    return globalresources::get().resourceheap().addsrv(srvdesc, d3dresource.Get());
}

uav texture::createuav() const
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
    uavdesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    return globalresources::get().resourceheap().adduav(uavdesc, d3dresource.Get());
}

void texture_dynamic::createresource(stdx::vecui2 dims, std::vector<uint8_t> const& texturedata)
{
    // textures cannot be 0 sized
    // todo : min/max should be specailized for stdx::vec0
    // perhaps we should create dummy objects
    _dims = { std::max<uint>(dims[0], 1u), std::max<uint>(dims[1], 1u) };
    _bufferupload = create_perframeuploadbufferunmapped(size());
    _texture = createtexture_default(_dims[0], _dims[1], _format, D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
    srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvdesc.Format = _format;
    srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvdesc.Texture2D.MipLevels = 1;

    _srv = globalresources::get().resourceheap().addsrv(srvdesc, _texture.Get());
    updateresource(texturedata);
}

void texture_dynamic::updateresource(std::vector<uint8_t> const& texturedata)
{
    // use 4 bytes per texel for now
    stdx::cassert([&] { return texturedata.size() == _dims[0] * _dims[1] * 4; });
    D3D12_SUBRESOURCE_DATA subresdata;
    subresdata.pData = texturedata.data();
    subresdata.RowPitch = _dims[0] * 4;
    subresdata.SlicePitch = _dims[0] * _dims[1] * 4;

    updatesubres(_texture.Get(), _bufferupload.Get(), &subresdata);
}

D3D12_GPU_DESCRIPTOR_HANDLE texture_dynamic::deschandle() const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(globalresources::get().resourceheap().d3dheap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(_srv.heapidx * srvcbvuav_descincrementsize()));
}

uint texture_dynamic::size() const
{
    return _dims[0] * _dims[1] * dxgiformatsize(_format);
}

model::model(std::string const& objpath, bool translatetoorigin)
{
    rapidobj::Result result = rapidobj::ParseFile(std::filesystem::path(objpath), rapidobj::MaterialLibrary::Default(rapidobj::Load::Optional));
    stdx::cassert(!result.error);
    
    rapidobj::Triangulate(result);
    stdx::cassert(!result.error);

    for (auto const& shape : result.shapes)
    {
        auto const& objindices = shape.mesh.indices;
        for (uint i(0); i < shape.mesh.num_face_vertices.size(); ++i)
        {
            stdx::cassert(shape.mesh.num_face_vertices[i] == 3);
            indices.push_back(objindices[i * 3].position_index);
            indices.push_back(objindices[i * 3 + 1].position_index);
            indices.push_back(objindices[i * 3 + 2].position_index);
        }
    }

    auto const& positions = result.attributes.positions;

    // make sure there is no padding
    static_assert(sizeof(std::decay_t<decltype(positions)>::value_type) * 3 == sizeof(stdx::vec3));

    stdx::vec3 const* const vert_start = reinterpret_cast<stdx::vec3 const*>(positions.data());
    vertices = std::vector<stdx::vec3>(vert_start, vert_start + (positions.size() / 3));

    if (translatetoorigin)
    {
        geometry::aabb bounds;
        for (auto const& v : vertices) bounds += v;

        // translate to (0,0,0)
        auto const& center = bounds.center();
        for (auto& v : vertices) v -= center;
    }
}

void globalresources::init()
{
    CHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    _assetspath = assetsPath;

    addpso("lines", "default_as.cso", "lines_ms.cso", "basic_ps.cso");
    addpso("default", "default_as.cso", "default_ms.cso", "default_ps.cso");
    addpso("default_twosided", "default_as.cso", "default_ms.cso", "default_ps.cso", psoflags::twosided | psoflags::transparent);
    addpso("texturess", "", "texturess_ms.cso", "texturess_ps.cso");
    addpso("instancedlines", "default_as.cso", "linesinstances_ms.cso", "basic_ps.cso");
    addpso("instanced", "default_as.cso", "instances_ms.cso", "instances_ps.cso");
    addpso("transparent", "default_as.cso", "default_ms.cso", "default_ps.cso", psoflags::transparent);
    addpso("transparent_twosided", "default_as.cso", "default_ms.cso", "default_ps.cso", psoflags::transparent | psoflags::twosided);
    addpso("wireframe", "default_as.cso", "default_ms.cso", "default_ps.cso", psoflags::wireframe | psoflags::transparent);
    addpso("instancedtransparent", "default_as.cso", "instances_ms.cso", "instances_ps.cso", psoflags::transparent);

    addcomputepso("blend", "blend_cs.cso");

    addmat("black", material().diffuse(color::black));  

    addmat("white", material().diffuse(color::white));
    addmat("red", material().diffuse(color::red));
    addmat("water", material().diffuse(color::water));

    _resourceheap.d3dheap = createresourcedescriptorheap();
    _cbuffer.createresource();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
    srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvdesc.Format = _rendertarget->GetDesc().Format;
    srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvdesc.Texture2D.MipLevels = 1;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
    uavdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavdesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    // render target srv is at slot 0
    // render target uav is at slot 1
    _resourceheap.addsrv(srvdesc, _rendertarget.Get());
    _resourceheap.adduav(uavdesc, _rendertarget.Get());
}

viewinfo& globalresources::view() { return _view; }
psomapref globalresources::psomap() const { return _psos; }
matmapref globalresources::matmap() const { return _materials; }
materialcref globalresources::defaultmat() const { return _defaultmat; }
constantbuffer<sceneconstants>& globalresources::cbuffer() { return _cbuffer; }
void globalresources::rendertarget(ComPtr<ID3D12Resource>& rendertarget) { _rendertarget = rendertarget; }
ComPtr<ID3D12Resource>& globalresources::rendertarget() { return _rendertarget; }
resourceheap& globalresources::resourceheap() { return _resourceheap; }
ComPtr<ID3D12Device5>& globalresources::device() { return _device; }
ComPtr<ID3D12GraphicsCommandList6>& globalresources::cmdlist() { return _commandlist; }
void globalresources::frameindex(uint idx) { _frameindex = idx; }
uint globalresources::frameindex() const { return _frameindex; }
std::string globalresources::assetfullpath(std::string const& path) const { return _assetspath + path; }
void globalresources::psodesc(D3DX12_MESH_SHADER_PIPELINE_STATE_DESC const& psodesc) { _psodesc = psodesc; }
materialcref globalresources::addmat(std::string const& name, material const& mat, bool twosided) { return _materials.insert({ name, {mat, twosided} }).first->second; }

uint globalresources::dxgisize(DXGI_FORMAT format)
{
    return _dxgisizes.contains(format) ? _dxgisizes[format] : stdx::invalid<uint>();
}

materialcref globalresources::mat(std::string const& name)
{
    if (auto const& found = _materials.find(name); found != _materials.end())
        return found->second;

    return _defaultmat;
}

void globalresources::addcomputepso(std::string const& name, std::string const& cs)
{
    if (_psos.find(name) != _psos.cend())
    {
        stdx::cassert(false, "trying to add pso of speciifed name when it already exists");
    }

    shader computeshader;
    if (!cs.empty())
        ReadDataFromFile(assetfullpath(cs).c_str(), &computeshader.data, &computeshader.size);

    // pull root signature from the precompiled compute shader
    // todo : share root signatures?
    ComPtr<ID3D12RootSignature>& rootsig = _rootsig.emplace_back();
    ThrowIfFailed(_device->CreateRootSignature(0, computeshader.data, computeshader.size, IID_PPV_ARGS(rootsig.GetAddressOf())));
    _psos[name].root_signature = rootsig;

    D3D12_COMPUTE_PIPELINE_STATE_DESC psodesc = {};
    psodesc.pRootSignature = rootsig.Get();
    psodesc.CS = { computeshader.data, computeshader.size };

    auto psostream = CD3DX12_PIPELINE_STATE_STREAM2(psodesc);

    D3D12_PIPELINE_STATE_STREAM_DESC stream_desc;
    stream_desc.pPipelineStateSubobjectStream = &psostream;
    stream_desc.SizeInBytes = sizeof(psostream);

    ThrowIfFailed(_device->CreatePipelineState(&stream_desc, IID_PPV_ARGS(_psos[name].pso.GetAddressOf())));
}

void globalresources::addpso(std::string const& name, std::string const& as, std::string const& ms, std::string const& ps, uint flags)
{
    if (_psos.find(name) != _psos.cend())
    {
        stdx::cassert(false, "trying to add pso of speciifed name when it already exists");
    }

    shader ampshader, meshshader, pixelshader;

    if (!as.empty())
        ReadDataFromFile(assetfullpath(as).c_str(), &ampshader.data, &ampshader.size);

    ReadDataFromFile(assetfullpath(ms).c_str(), &meshshader.data, &meshshader.size);
    ReadDataFromFile(assetfullpath(ps).c_str(), &pixelshader.data, &pixelshader.size);
    
    // pull root signature from the precompiled mesh shaders.
    // todo : share root signatures?
    ComPtr<ID3D12RootSignature>& rootsig = _rootsig.emplace_back();
    ThrowIfFailed(_device->CreateRootSignature(0, meshshader.data, meshshader.size, IID_PPV_ARGS(rootsig.GetAddressOf())));
    _psos[name].root_signature = rootsig;

    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC pso_desc = _psodesc;
    pso_desc.pRootSignature = _psos[name].root_signature.Get();

    if (!as.empty())
        pso_desc.AS = { ampshader.data, ampshader.size };

    pso_desc.MS = { meshshader.data, meshshader.size };
    pso_desc.PS = { pixelshader.data, pixelshader.size };

    if (flags & psoflags::transparent)
    {
        D3D12_RENDER_TARGET_BLEND_DESC transparency_blenddesc = CD3DX12_RENDER_TARGET_BLEND_DESC(D3D12_DEFAULT);
        transparency_blenddesc.BlendEnable = true;
        transparency_blenddesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        transparency_blenddesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

        pso_desc.BlendState.RenderTarget[0] = transparency_blenddesc;
    }

    auto psostream = CD3DX12_PIPELINE_MESH_STATE_STREAM(pso_desc);

    D3D12_PIPELINE_STATE_STREAM_DESC stream_desc;
    stream_desc.pPipelineStateSubobjectStream = &psostream;
    stream_desc.SizeInBytes = sizeof(psostream);

    ThrowIfFailed(_device->CreatePipelineState(&stream_desc, IID_PPV_ARGS(_psos[name].pso.GetAddressOf())));

    if (flags & psoflags::wireframe)
    {
        D3DX12_MESH_SHADER_PIPELINE_STATE_DESC wireframepso = pso_desc;
        wireframepso.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        wireframepso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        auto psostream = CD3DX12_PIPELINE_MESH_STATE_STREAM(wireframepso);
        D3D12_PIPELINE_STATE_STREAM_DESC stream_desc;
        stream_desc.pPipelineStateSubobjectStream = &psostream;
        stream_desc.SizeInBytes = sizeof(psostream);
        ThrowIfFailed(_device->CreatePipelineState(&stream_desc, IID_PPV_ARGS(_psos[name].pso_wireframe.GetAddressOf())));
    }

    if (flags & psoflags::twosided)
    {
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        auto psostream_twosides = CD3DX12_PIPELINE_MESH_STATE_STREAM(pso_desc);

        D3D12_PIPELINE_STATE_STREAM_DESC stream_desc_twosides;
        stream_desc_twosides.pPipelineStateSubobjectStream = &psostream_twosides;
        stream_desc_twosides.SizeInBytes = sizeof(psostream_twosides);

        ThrowIfFailed(_device->CreatePipelineState(&stream_desc_twosides, IID_PPV_ARGS(_psos[name].pso_twosided.GetAddressOf())));
    }
}

void SerializeAndCreateRaytracingRootSignature(Microsoft::WRL::ComPtr<ID3D12Device5>& device, D3D12_ROOT_SIGNATURE_DESC& desc, Microsoft::WRL::ComPtr<ID3D12RootSignature>* rootSig)
{
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

gfx::pipeline_objects& globalresources::addraytracingpso(std::string const& name, std::string const& libname, raytraceshaders const& shaders)
{
    if (auto existing = _psos.find(name); existing != _psos.cend())
    {
        stdx::cassert(false, "trying to add pso of speciifed name when it already exists");
        return _psos[name];
    }

    // global root signature is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    
    {
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootsig;
        CD3DX12_ROOT_SIGNATURE_DESC rootsig_desc(0, nullptr);
        rootsig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

        // empty root signature
        SerializeAndCreateRaytracingRootSignature(_device, rootsig_desc, &rootsig);

        _psos[name].root_signature = rootsig;
    }

    // empty local root signature
    {
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;

        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        SerializeAndCreateRaytracingRootSignature(_device, localRootSignatureDesc, &rootSig);

        _psos[name].rootsignature_local = rootSig;
    }

    shader raytracinglib;
    ReadDataFromFile(assetfullpath(libname).c_str(), &raytracinglib.data, &raytracinglib.size);

    CD3DX12_STATE_OBJECT_DESC raytracingpipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    // DXIL library
    // this contains the shaders and their entrypoints for the state object.
    // since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
    auto lib = raytracingpipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)raytracinglib.data, raytracinglib.size);
    lib->SetDXILLibrary(&libdxil);

    auto const& trihitgrp = shaders.tri_hitgrp;
    auto const& prochitgrp = shaders.procedural_hitgroup;

    if(!trihitgrp.name.empty())
    {
        // we do not define any exports so all shaders will be exported
        auto hitGroup = raytracingpipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetClosestHitShaderImport(utils::strtowstr(shaders.tri_hitgrp.closesthit).c_str());
        hitGroup->SetHitGroupExport(utils::strtowstr(shaders.tri_hitgrp.name).c_str());
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    }

    if(!prochitgrp.name.empty())
    {
        auto hitGroup = raytracingpipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetIntersectionShaderImport(utils::strtowstr(prochitgrp.intersection).c_str());

        if (!prochitgrp.anyhit.empty())
            hitGroup->SetAnyHitShaderImport(utils::strtowstr(prochitgrp.anyhit).c_str());

        hitGroup->SetClosestHitShaderImport(utils::strtowstr(prochitgrp.closesthit).c_str());
        hitGroup->SetHitGroupExport(utils::strtowstr(prochitgrp.name).c_str());
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
    }

    // defines the maximum sizes in bytes for the ray payload and attribute structure.
    auto shaderConfig = raytracingpipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    
    UINT payloadSize = 4 * sizeof(float);   // float4 color
    UINT attributeSize = 2 * sizeof(float); // float2 for barycentrics
    shaderConfig->Config(payloadSize, attributeSize);

    // empty local root signature
    auto localrootsignature = raytracingpipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    localrootsignature->SetRootSignature(_psos[name].rootsignature_local.Get());

    // global root signature
    // this is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    auto globalrootsig = raytracingpipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalrootsig->SetRootSignature(_psos[name].root_signature.Get());

    auto pipelineconfig = raytracingpipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipelineconfig->Config(1); // primary rays only. 

    // create the state object.
    ThrowIfFailed(_device->CreateStateObject(raytracingpipeline, IID_PPV_ARGS(&_psos[name].pso_raytracing)));

    return _psos[name];
}

globalresources& globalresources::get()
{
    static globalresources res;
    return res;
}

std::string generaterandom_matcolor(stdx::ext<material, bool> definition, std::optional<std::string> const& preferred_name)
{
    std::random_device rd;
    static std::mt19937 re(rd());
    static constexpr uint matgenlimit = 10000u;
    static std::uniform_int_distribution<uint> matnumberdist(0, matgenlimit);
    static std::uniform_real_distribution<float> colordist(0.f, 1.f);

    vector3 const color = { colordist(re), colordist(re), colordist(re) };
    std::string basename("mat");

    std::string matname = (preferred_name.has_value() ? preferred_name.value() : basename) + std::to_string(matnumberdist(re));

    while (globalresources::get().matmap().find(matname) != globalresources::get().matmap().end()) { matname = basename + std::to_string(matnumberdist(re)); }

    definition->a = vector4(color.x, color.y, color.z, definition->a.w);
    globalresources::get().addmat(matname, definition, definition.ex());
    return matname;
}

void update_allframebuffers(std::byte* mapped_buffer, void const* data_start, uint const perframe_buffersize)
{
    for (uint i = 0; i < frame_count; ++i)
        memcpy(mapped_buffer + perframe_buffersize * i, data_start, perframe_buffersize);
}

cbv createcbv(uint size, ID3D12Resource* res)
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvdesc = {};
    cbvdesc.SizeInBytes = UINT(size);
    cbvdesc.BufferLocation = res->GetGPUVirtualAddress();

    return globalresources::get().resourceheap().addcbv(cbvdesc, res);
}

uint srvcbvuav_descincrementsize()
{
    auto device = globalresources::get().device();
    return static_cast<uint>(device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
}

uint dxgiformatsize(DXGI_FORMAT format)
{
    return globalresources::get().dxgisize(format);
}

ComPtr<ID3D12Resource> create_uploadbuffer(std::byte** mapped_buffer, uint const buffersize)
{
    auto device = globalresources::get().device();

    ComPtr<ID3D12Resource> b_upload;
    if (buffersize > 0)
    {
        auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(buffersize);
        auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(b_upload.GetAddressOf())));

        // we do not intend to read from this resource on the CPU.
        ThrowIfFailed(b_upload->Map(0, nullptr, reinterpret_cast<void**>(mapped_buffer)));
    }

    return b_upload;
}

ComPtr<ID3D12Resource> create_perframeuploadbuffers(std::byte** mapped_buffer, uint const buffersize)
{
    auto device = globalresources::get().device();

    ComPtr<ID3D12Resource> b_upload;
    if (buffersize > 0)
    {
        auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(frame_count * buffersize);
        auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(b_upload.GetAddressOf())));

        // we do not intend to read from this resource on the CPU.
        ThrowIfFailed(b_upload->Map(0, nullptr, reinterpret_cast<void**>(mapped_buffer)));
    }

    return b_upload;
}

ComPtr<ID3D12Resource> create_uploadbufferwithdata(void const* data_start, uint const buffersize)
{
    // create upload buffer and copy data into it
    std::byte* _mappeddata = nullptr;
    ComPtr<ID3D12Resource> uploadresource = create_uploadbuffer(&_mappeddata, buffersize);
    memcpy(_mappeddata, data_start, buffersize);

    return uploadresource;
}

ComPtr<ID3D12Resource> create_perframeuploadbufferunmapped(uint const buffersize)
{
    auto device = globalresources::get().device();

    ComPtr<ID3D12Resource> b_upload;
    if (buffersize > 0)
    {
        auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(frame_count * buffersize);
        auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(b_upload.GetAddressOf())));
    }

    return b_upload;
}

ComPtr<ID3D12DescriptorHeap> createresourcedescriptorheap()
{
    auto device = globalresources::get().device();

    D3D12_DESCRIPTOR_HEAP_DESC heapdesc = {};
    heapdesc.NumDescriptors = 100; // todo : arbitrary limit, make a class for resource heap
    heapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> heap;
    ThrowIfFailed(device->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf())));
    return heap;
}

srv createsrv(D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc, ID3D12Resource* resource)
{
    return globalresources::get().resourceheap().addsrv(srvdesc, resource);
}

uav createuav(D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc, ID3D12Resource* resource)
{
    return globalresources::get().resourceheap().adduav(uavdesc, resource);
}

void createsrv(D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc, ID3D12Resource* resource, ID3D12DescriptorHeap* resourceheap, uint heapslot)
{
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(resourceheap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(heapslot * srvcbvuav_descincrementsize()));
    device->CreateShaderResourceView(resource, &srvdesc, deschandle);
}

ComPtr<ID3D12Resource> create_default_uavbuffer(std::size_t const b_size)
{
    auto device = globalresources::get().device();
    auto b_desc = CD3DX12_RESOURCE_DESC::Buffer(b_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    ComPtr<ID3D12Resource> b;

    // create buffer on the default heap
    auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &b_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(b.ReleaseAndGetAddressOf())));
    return b;
}

ComPtr<ID3D12Resource> create_accelerationstructbuffer(std::size_t const b_size)
{
    auto device = globalresources::get().device();
    auto b_desc = CD3DX12_RESOURCE_DESC::Buffer(b_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    ComPtr<ID3D12Resource> b;

    // create buffer on the default heap
    auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &b_desc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(b.ReleaseAndGetAddressOf())));
    return b;
}

default_and_upload_buffers create_defaultbuffer(void const* datastart, std::size_t const b_size)
{
    auto device = globalresources::get().device();

    ComPtr<ID3D12Resource> b;
    ComPtr<ID3D12Resource> b_upload;

    if (b_size > 0)
    {
        // allow unordered access on default buffers, for convenience
        auto b_desc = CD3DX12_RESOURCE_DESC::Buffer(b_size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto ub_desc = CD3DX12_RESOURCE_DESC::Buffer(b_size);

        // create buffer on the default heap
        auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &b_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(b.ReleaseAndGetAddressOf())));

        // create resource on the upload heap
        auto uploadheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&uploadheap_desc, D3D12_HEAP_FLAG_NONE, &ub_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(b_upload.GetAddressOf())));

        {
            uint8_t* b_upload_start = nullptr;

            // we do not intend to read from this resource on the CPU.
            b_upload->Map(0, nullptr, reinterpret_cast<void**>(&b_upload_start));

            // copy data to upload heap
            memcpy(b_upload_start, datastart, b_size);

            b_upload->Unmap(0, nullptr);
        }

        auto resource_transition = CD3DX12_RESOURCE_BARRIER::Transition(b.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        auto cmdlist = globalresources::get().cmdlist();

        // copy data from upload heap to default heap
        cmdlist->CopyResource(b.Get(), b_upload.Get());
        cmdlist->ResourceBarrier(1, &resource_transition);
    }

    return { b, b_upload };
}

ComPtr<ID3D12Resource> createtexture_default(uint width, uint height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state)
{
    auto device = globalresources::get().device();

    // all unordered access on all textures(on some architecutres unordered access might lead to suboptimal texture layout, but its 2025)
    auto texdesc = CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<UINT64>(width), static_cast<UINT>(height), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    ComPtr<ID3D12Resource> texdefault;
    auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &texdesc, state, nullptr, IID_PPV_ARGS(texdefault.ReleaseAndGetAddressOf())));
    return texdefault;
}

void update_buffer(std::byte* mapped_buffer, void const* data_start, std::size_t const data_size)
{
    memcpy(mapped_buffer, data_start, data_size);
}

uint updatesubres(ID3D12Resource* dest, ID3D12Resource* upload, D3D12_SUBRESOURCE_DATA const* srcdata)
{
    auto cmdlist = globalresources::get().cmdlist();
    return UpdateSubresources(cmdlist.Get(), dest, upload, 0, 0, 1, srcdata);
}

D3D12_GPU_VIRTUAL_ADDRESS get_perframe_gpuaddress(D3D12_GPU_VIRTUAL_ADDRESS start, std::size_t perframe_buffersize)
{
    auto frame_idx = globalresources::get().frameindex();
    return start + perframe_buffersize * frame_idx;
}

void update_currframebuffer(std::byte* mapped_buffer, void const* data_start, std::size_t const data_size, std::size_t const perframe_buffersize)
{
    auto frame_idx = globalresources::get().frameindex();
    memcpy(mapped_buffer + perframe_buffersize * frame_idx, data_start, data_size);
}

}