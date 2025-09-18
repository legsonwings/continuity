module;

#include "thirdparty/dxhelpers.h"
#include "thirdparty/d3dx12.h"

module graphics:globalresources;

//import vec;
import engine;

using Microsoft::WRL::ComPtr;

namespace gfx
{

void serialize_create_rootsignature(ComPtr<ID3D12Device5>& device, D3D12_ROOT_SIGNATURE_DESC const& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error;

    ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
    ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

void globalresources::init()
{
    CHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    _assetspath = assetsPath;

    //addpso("lines", "default_as.cso", "lines_ms.cso", "basic_ps.cso");
    //addpso("default", "default_as.cso", "default_ms.cso", "default_ps.cso");
    //addpso("default_twosided", "default_as.cso", "default_ms.cso", "default_ps.cso", psoflags::twosided | psoflags::transparent);
    //addpso("texturess", "", "texturess_ms.cso", "texturess_ps.cso");
    //addpso("instancedlines", "default_as.cso", "linesinstances_ms.cso", "basic_ps.cso");
    addpso("instanced", "", "instances_ms.cso", "instances_ps.cso");
    addpso("instanced_depthonly", "", "instancesdepthonly_ms.cso", "");
    //addpso("transparent", "default_as.cso", "default_ms.cso", "default_ps.cso", psoflags::transparent);
    //addpso("transparent_twosided", "default_as.cso", "default_ms.cso", "default_ps.cso", psoflags::transparent | psoflags::twosided);
    //addpso("wireframe", "default_as.cso", "default_ms.cso", "default_ps.cso", psoflags::wireframe | psoflags::transparent);
    //addpso("instancedtransparent", "default_as.cso", "instances_ms.cso", "instances_ps.cso", psoflags::transparent);

    addcomputepso("blend", "blend_cs.cso");
    addcomputepso("genmipmaps", "genmipmaps_cs.cso");
    addcomputepso("temporalaccum", "temporalaccum_cs.cso");
    addcomputepso("tonemap", "tonemap_cs.cso");

    addmat(material().colour(color::black));
    addmat(material().colour(color::white));
    addmat(material().colour(color::red));
    addmat(material().colour(color::water));
}

void globalresources::deinit()
{
    get() = {};
}

void globalresources::create_resources()
{
    materialsbuffer.create(_materials);
    _materialsbuffer_idx = materialsbuffer.createsrv().heapidx;

    D3D12_SAMPLER_DESC samplerdesc = {};
    samplerdesc.Filter = D3D12_FILTER_ANISOTROPIC;
    samplerdesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerdesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerdesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerdesc.MinLOD = 0;
    samplerdesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerdesc.MipLODBias = 0.0f;
    samplerdesc.MaxAnisotropy = 16;
    samplerdesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplerdesc.BorderColor[0] = 0.0f;
    samplerdesc.BorderColor[1] = 0.0f;
    samplerdesc.BorderColor[2] = 0.0f;
    samplerdesc.BorderColor[3] = 1.0f;

    // create a aniso sampler at 0 for now
    auto samplerview = _samplerheap.addsampler(samplerdesc);
    stdx::cassert(samplerview.heapidx == 0);

    samplerdesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    samplerdesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerdesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerdesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerdesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerdesc.MaxAnisotropy = 0;

    // create a linear sampler at 1 for now
    samplerview = _samplerheap.addsampler(samplerdesc);
    stdx::cassert(samplerview.heapidx == 1);

    samplerdesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

    // create a point sampler at 2
    samplerview = _samplerheap.addsampler(samplerdesc);
    stdx::cassert(samplerview.heapidx == 2);

    samplerdesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplerdesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;

    // create a shadow map sampler at 3
    samplerview = _samplerheap.addsampler(samplerdesc);
    stdx::cassert(samplerview.heapidx == 3);
}

viewinfo& globalresources::view() { return _view; }
psomapref globalresources::psomap() const { return _psos; }
//matmapref globalresources::matmap() const { return _materials; }
uint32 globalresources::materialsbuffer_idx() const { return _materialsbuffer_idx; }
void globalresources::rendertarget(ComPtr<ID3D12Resource>& rendertarget) { _rendertarget = rendertarget; }
ComPtr<ID3D12Resource>& globalresources::rendertarget() { return _rendertarget; }
resourceheap& globalresources::resourceheap() { return _resourceheap; }
samplerheap& globalresources::samplerheap() { return _samplerheap; }
ComPtr<ID3D12Device5>& globalresources::device() { return _device; }
ComPtr<ID3D12GraphicsCommandList6>& globalresources::cmdlist() { return _commandlist; }
std::string globalresources::assetfullpath(std::string const& path) const { return _assetspath + path; }
void globalresources::psodesc(D3DX12_MESH_SHADER_PIPELINE_STATE_DESC const& psodesc) { _psodesc = psodesc; }

uint32 globalresources::addmat(material const& mat)
{
    stdx::cassert(_materials.size() < max_materials);
    _materials.push_back(mat);
    return uint32(_materials.size() - 1);
}

uint globalresources::dxgisize(DXGI_FORMAT format)
{
    return _dxgisizes.contains(format) ? _dxgisizes[format] : stdx::invalid<uint>;
}

material& globalresources::mat(uint32 id)
{
    return _materials[id];
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

    CD3DX12_ROOT_PARAMETER rootparam;
    rootparam.InitAsConstants(3, 0);
    CD3DX12_ROOT_SIGNATURE_DESC rootsig_desc(1, &rootparam);
    rootsig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

    // todo : shouldn't need to create same root signature for every pso(can even be shared among graphics and compute)
    ComPtr<ID3D12RootSignature> rootsig;
    serialize_create_rootsignature(_device, rootsig_desc, &rootsig);
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
    if (!ps.empty())
        ReadDataFromFile(assetfullpath(ps).c_str(), &pixelshader.data, &pixelshader.size);

    CD3DX12_ROOT_PARAMETER rootparam;
    rootparam.InitAsConstants(3, 0);
    CD3DX12_ROOT_SIGNATURE_DESC rootsig_desc(1, &rootparam);
    rootsig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

    // todo : shouldn't need to create same root signature for every pso
    ComPtr<ID3D12RootSignature> rootsig;
    serialize_create_rootsignature(_device, rootsig_desc, &rootsig);

    _psos[name].root_signature = rootsig;

    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC pso_desc = _psodesc;
    pso_desc.pRootSignature = _psos[name].root_signature.Get();

    if (!as.empty())
        pso_desc.AS = { ampshader.data, ampshader.size };

    pso_desc.MS = { meshshader.data, meshshader.size };

    if (!ps.empty())
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

gfx::pipeline_objects& globalresources::addraytracingpso(std::string const& name, std::string const& libname, raytraceshaders const& shaders)
{
    if (auto existing = _psos.find(name); existing != _psos.cend())
    {
        stdx::cassert(false, "trying to add pso of speciifed name when it already exists");
        return _psos[name];
    }

    // global root signature is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.

    {
        // todo : everything uses same root signature
        // make a sharable one
        ComPtr<ID3D12RootSignature> rootsig;

        CD3DX12_ROOT_PARAMETER rootparam;
        rootparam.InitAsConstants(1, 0);
        CD3DX12_ROOT_SIGNATURE_DESC rootsig_desc(1, &rootparam);
        rootsig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

        // empty root signature
        serialize_create_rootsignature(_device, rootsig_desc, &rootsig);

        _psos[name].root_signature = rootsig;
    }

    // empty local root signature
    {
        ComPtr<ID3D12RootSignature> rootSig;

        CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
        serialize_create_rootsignature(_device, localRootSignatureDesc, &rootSig);

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

    if (!trihitgrp.name.empty())
    {
        // we do not define any exports so all shaders will be exported
        auto hitGroup = raytracingpipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetClosestHitShaderImport(utils::strtowstr(shaders.tri_hitgrp.closesthit).c_str());
        hitGroup->SetHitGroupExport(utils::strtowstr(shaders.tri_hitgrp.name).c_str());
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
    }

    if (!prochitgrp.name.empty())
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

    UINT payloadSize = 3 * sizeof(float) + 2 * sizeof(uint32);
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
    pipelineconfig->Config(31); // primary rays only. 

    // create the state object.
    ThrowIfFailed(_device->CreateStateObject(raytracingpipeline, IID_PPV_ARGS(&_psos[name].pso_raytracing)));

    return _psos[name];
}

globalresources& globalresources::get()
{
    static globalresources res;
    return res;
}

}