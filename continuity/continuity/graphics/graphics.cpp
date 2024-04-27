module;

#include <wrl.h>
#include <d3d12.h>
#include "thirdparty/dxhelpers.h"
#include "thirdparty/d3dx12.h"

module graphics;

// seems like the declarations is not available across files
using Microsoft::WRL::ComPtr;

static constexpr uint frame_count = 2;

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
    _bufferupload = create_uploadbufferunmapped(size());
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
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(_srvheap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(_heapidx * srvsbvuav_descincrementsize()));
}

uint texture::size() const
{ 
    return _dims[0] * _dims[1] * dxgiformatsize(_format);
}

void globalresources::init()
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    _assetspath = assetsPath;

    addpso("lines", L"default_as.cso", L"lines_ms.cso", L"basic_ps.cso");
    addpso("default", L"default_as.cso", L"default_ms.cso", L"default_ps.cso");
    addpso("default_twosided", L"default_as.cso", L"default_ms.cso", L"default_ps.cso", psoflags::twosided | psoflags::transparent);
    addpso("texturess", L"", L"texturess_ms.cso", L"texturess_ps.cso");
    addpso("instancedlines", L"default_as.cso", L"linesinstances_ms.cso", L"basic_ps.cso");
    addpso("instanced", L"default_as.cso", L"instances_ms.cso", L"instances_ps.cso");
    addpso("transparent", L"default_as.cso", L"default_ms.cso", L"default_ps.cso", psoflags::transparent);
    addpso("transparent_twosided", L"default_as.cso", L"default_ms.cso", L"default_ps.cso", psoflags::transparent | psoflags::twosided);
    addpso("wireframe", L"default_as.cso", L"default_ms.cso", L"default_ps.cso", psoflags::wireframe | psoflags::transparent);
    addpso("instancedtransparent", L"default_as.cso", L"instances_ms.cso", L"instances_ps.cso", psoflags::transparent);

    addmat("black", material().diffuse(color::black));  

    addmat("white", material().diffuse(color::white));
    addmat("red", material().diffuse(color::red));
    addmat("water", material().diffuse(color::water));

    D3D12_DESCRIPTOR_HEAP_DESC srvheapdesc = {};
    srvheapdesc.NumDescriptors = 1;
    srvheapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvheapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    _srvheap = createsrvdescriptorheap(srvheapdesc);
    _cbuffer.createresource();
}

viewinfo& globalresources::view() { return _view; }
psomapref globalresources::psomap() const { return _psos; }
matmapref globalresources::matmap() const { return _materials; }
materialcref globalresources::defaultmat() const { return _defaultmat; }
constantbuffer<sceneconstants>& globalresources::cbuffer() { return _cbuffer; }
ComPtr<ID3D12DescriptorHeap>& globalresources::srvheap() { return _srvheap; }
ComPtr<ID3D12Device2>& globalresources::device() { return _device; }
ComPtr<ID3D12GraphicsCommandList6>& globalresources::cmdlist() { return _commandlist; }
void globalresources::frameindex(uint idx) { _frameindex = idx; }
uint globalresources::frameindex() const { return _frameindex; }
std::wstring globalresources::assetfullpath(std::wstring const& path) const { return _assetspath + path; }
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

void globalresources::addpso(std::string const& name, std::wstring const& as, std::wstring const& ms, std::wstring const& ps, uint flags)
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
    if (!_rootsig) ThrowIfFailed(_device->CreateRootSignature(0, meshshader.data, meshshader.size, IID_PPV_ARGS(_rootsig.GetAddressOf())));
    _psos[name].root_signature = _rootsig;

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

uint srvsbvuav_descincrementsize()
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
        auto resource_desc = CD3DX12_RESOURCE_DESC::Buffer(frame_count * buffersize);
        auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(b_upload.GetAddressOf())));

        // we do not intend to read from this resource on the CPU.
        ThrowIfFailed(b_upload->Map(0, nullptr, reinterpret_cast<void**>(mapped_buffer)));
    }

    return b_upload;
}

ComPtr<ID3D12Resource> create_uploadbufferunmapped(uint const buffersize)
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

ComPtr<ID3D12DescriptorHeap> createsrvdescriptorheap(D3D12_DESCRIPTOR_HEAP_DESC heapdesc)
{
    auto device = globalresources::get().device();

    ComPtr<ID3D12DescriptorHeap> heap;
    ThrowIfFailed(device->CreateDescriptorHeap(&heapdesc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf())));
    return heap;
}

void createsrv(D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc, ID3D12Resource* resource, ID3D12DescriptorHeap* srvheap, uint heapslot)
{
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(srvheap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(heapslot * srvsbvuav_descincrementsize()));
    device->CreateShaderResourceView(resource, &srvdesc, deschandle);
}

default_and_upload_buffers create_defaultbuffer(void const* datastart, std::size_t const vb_size)
{
    auto device = globalresources::get().device();

    ComPtr<ID3D12Resource> vb;
    ComPtr<ID3D12Resource> vb_upload;

    if (vb_size > 0)
    {
        auto vb_desc = CD3DX12_RESOURCE_DESC::Buffer(vb_size);

        // create vertex buffer on the default heap
        auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &vb_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(vb.ReleaseAndGetAddressOf())));

        // Create vertex resource on the upload heap
        auto uploadheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        ThrowIfFailed(device->CreateCommittedResource(&uploadheap_desc, D3D12_HEAP_FLAG_NONE, &vb_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(vb_upload.GetAddressOf())));

        {
            uint8_t* vb_upload_start = nullptr;

            // we do not intend to read from this resource on the CPU.
            vb_upload->Map(0, nullptr, reinterpret_cast<void**>(&vb_upload_start));

            // copy vertex data to upload heap
            memcpy(vb_upload_start, datastart, vb_size);

            vb_upload->Unmap(0, nullptr);
        }

        auto resource_transition = CD3DX12_RESOURCE_BARRIER::Transition(vb.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        auto cmdlist = globalresources::get().cmdlist();

        // copy vertex data from upload heap to default heap
        cmdlist->CopyResource(vb.Get(), vb_upload.Get());
        cmdlist->ResourceBarrier(1, &resource_transition);
    }

    return { vb, vb_upload };
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

void update_perframebuffer(std::byte* mapped_buffer, void const* data_start, std::size_t const data_size, std::size_t const perframe_buffersize)
{
    auto frame_idx = globalresources::get().frameindex();
    memcpy(mapped_buffer + perframe_buffersize * frame_idx, data_start, data_size);
}

}