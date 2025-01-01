module;

#include <wrl.h>
#include <d3d12.h>
#include "thirdparty/dxhelpers.h"
#include "thirdparty/d3dx12.h"

module graphics;

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

void texture::createresource(uint heapidx, stdx::vecui2 dims, std::vector<uint8_t> const& texturedata, ID3D12DescriptorHeap* srvheap)
{
    // textures cannot be 0 sized
    // todo : min/max should be specailized for stdx::vec0
    // perhaps we should create dummy objects
    _dims = { std::max<uint>(dims[0], 1u), std::max<uint>(dims[1], 1u) };
    _heapidx = heapidx;
    _srvheap = srvheap;
    _bufferupload = create_perframeuploadbufferunmapped(size());
    _texture = createtexture_default(_dims[0], _dims[1], _format);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
    srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvdesc.Format = _format;
    srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvdesc.Texture2D.MipLevels = 1;

    createsrv(srvdesc, _texture.Get(), _srvheap.Get(), _heapidx);
    updateresource(texturedata);
}

void texture::updateresource(std::vector<uint8_t> const& texturedata)
{
    // use 4 bytes per texel for now
    stdx::cassert([&] { return texturedata.size() == _dims[0] * _dims[1] * 4; });
    D3D12_SUBRESOURCE_DATA subresdata;
    subresdata.pData = texturedata.data();
    subresdata.RowPitch = _dims[0] * 4;
    subresdata.SlicePitch = _dims[0] * _dims[1] * 4;

    updatesubres(_texture.Get(), _bufferupload.Get(), &subresdata);
}

D3D12_GPU_DESCRIPTOR_HANDLE texture::deschandle() const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(_srvheap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(_heapidx * srvcbvuav_descincrementsize()));
}

uint texture::size() const
{ 
    return _dims[0] * _dims[1] * dxgiformatsize(_format);
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

    addmat("black", material().diffuse(color::black));  

    addmat("white", material().diffuse(color::white));
    addmat("red", material().diffuse(color::red));
    addmat("water", material().diffuse(color::water));

    _srvheap = createresourcedescriptorheap();
    _cbuffer.createresource();
}

viewinfo& globalresources::view() { return _view; }
psomapref globalresources::psomap() const { return _psos; }
matmapref globalresources::matmap() const { return _materials; }
materialcref globalresources::defaultmat() const { return _defaultmat; }
constantbuffer<sceneconstants>& globalresources::cbuffer() { return _cbuffer; }
void globalresources::rendertarget(ComPtr<ID3D12Resource>& rendertarget) { _rendertarget = rendertarget; }
ComPtr<ID3D12Resource>& globalresources::rendertarget() { return _rendertarget; }
ComPtr<ID3D12DescriptorHeap>& globalresources::srvheap() { return _srvheap; }
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

    // Global Root Signature
    // This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    
    // todo : create helper classes for root signature
    {
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;

        CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
        UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
        CD3DX12_ROOT_PARAMETER rootParameters[3];
        rootParameters[0].InitAsDescriptorTable(1, &UAVDescriptor);     // colour output uav
        rootParameters[1].InitAsShaderResourceView(0);                  // acceleration structire
        rootParameters[2].InitAsConstantBufferView(0);                  // constants
        CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
        SerializeAndCreateRaytracingRootSignature(_device, globalRootSignatureDesc, &rootSig);

        _psos[name].root_signature = rootSig;
    }

    // empty local Root Signature
    // This is a root signature that enables a shader to have unique arguments that come from shader tables.
    {
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;

        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc = {};
        localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
        SerializeAndCreateRaytracingRootSignature(_device, localRootSignatureDesc, &rootSig);

        _psos[name].rootsignature_local = rootSig;
    }

    shader raytracinglib;
    ReadDataFromFile(assetfullpath(libname).c_str(), &raytracinglib.data, &raytracinglib.size);

    // Create 7 subobjects that combine into a RTPSO:
   // Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
   // Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
   // This simple sample utilizes default shader association except for local root signature subobject
   // which has an explicit association specified purely for demonstration purposes.
   // 1 - DXIL library
   // 1 - Triangle hit group
   // 1 - Shader config
   // 2 - Local root signature and association
   // 1 - Global root signature
   // 1 - Pipeline config
    CD3DX12_STATE_OBJECT_DESC raytracingpipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    // DXIL library
    // This contains the shaders and their entrypoints for the state object.
    // Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
    auto lib = raytracingpipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void*)raytracinglib.data, raytracinglib.size);
    lib->SetDXILLibrary(&libdxil);
    // we do not define any exports so all shaders will be exported

    //lib->DefineExport(utils::strtowstr(shaders.raygen).c_str());
    //lib->DefineExport(utils::strtowstr(shaders.closesthit).c_str());
    //lib->DefineExport(utils::strtowstr(shaders.miss).c_str());

    {
        // Triangle hit group
        // A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
        // In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
        auto hitGroup = raytracingpipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetClosestHitShaderImport(utils::strtowstr(rtshaders.closesthit).c_str());
        hitGroup->SetHitGroupExport(utils::strtowstr(rtshaders.hitgroupname).c_str());
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    }

    {
        // procedural hit group
        auto const& prochitgrp = shaders.procedural_hitgroup;
        auto hitGroup = raytracingPipeline->CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetIntersectionShaderImport(utils::strtowstr(prochitgrp.intersection).c_str());

        if (!prochitgrp.anyhit.empty())
            hitGroup->SetAnyHitShaderImport(utils::strtowstr(prochitgrp.anyhit).c_str());

        hitGroup->SetClosestHitShaderImport(utils::strtowstr(prochitgrp.closesthit).c_str());
        hitGroup->SetHitGroupExport(utils::strtowstr(prochitgrp.name).c_str());
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_PROCEDURAL_PRIMITIVE);
    }

    // Shader config
    // Defines the maximum sizes in bytes for the ray payload and attribute structure.
    auto shaderConfig = raytracingpipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    
    UINT payloadSize = 4 * sizeof(float);   // float4 color
    //UINT attributeSize = 2 * sizeof(float) + 3 * sizeof(float); // float2 barycentrics(for triangles) + float3 normal(for procedural)
    shaderConfig->Config(payloadSize, 0);

    // hit group and miss shaders in this sample are not using a local root signature and thus one is not associated with them.
    // local root signature to be used in a ray gen shader.
    
    // todo : associate local sig with both our hit groups
    //auto localrootsignature = raytracingpipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
    //localrootsignature->SetRootSignature(_psos[name].rootsignature_local.Get());

    //// shader association
    //auto rootsignatureassociation = raytracingpipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
    //rootsignatureassociation->SetSubobjectToAssociate(*localrootsignature);
    //rootsignatureassociation->AddExport(utils::strtowstr(shaders.raygen).data());

    // global root signature
    // this is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
    auto globalrootsig = raytracingpipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    globalrootsig->SetRootSignature(_psos[name].root_signature.Get());

    auto pipelineconfig = raytracingpipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    pipelineconfig->Config(1); // primary rays only. 

    // Create the state object.
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

void createsrv(D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc, ID3D12Resource* resource, ID3D12DescriptorHeap* srvheap, uint heapslot)
{
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(srvheap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(heapslot * srvcbvuav_descincrementsize()));
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
        auto b_desc = CD3DX12_RESOURCE_DESC::Buffer(b_size);

        // create buffer on the default heap
        auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &b_desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(b.ReleaseAndGetAddressOf())));

        // create resource on the upload heap
        auto uploadheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&uploadheap_desc, D3D12_HEAP_FLAG_NONE, &b_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(b_upload.GetAddressOf())));

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

ComPtr<ID3D12Resource> createtexture_default(uint width, uint height, DXGI_FORMAT format)
{
    auto device = globalresources::get().device();
    auto texdesc = CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<UINT64>(width), static_cast<UINT>(height));

    ComPtr<ID3D12Resource> texdefault;
    auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &texdesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(texdefault.ReleaseAndGetAddressOf())));
    return texdefault;
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