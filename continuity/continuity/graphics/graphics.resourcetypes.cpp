module;

#include "thirdparty/d3dx12.h"
#include "thirdparty/dxhelpers.h"
#include "thirdparty/wictextureloader12.h"

module graphics:resourcetypes;

import engine;
import :globalresources;

namespace gfx
{

static constexpr uint32 frame_count = 2;

void update_allframebuffers(std::byte* mapped_buffer, void const* data_start, uint const perframe_buffersize)
{
    for (uint i = 0; i < frame_count; ++i)
        memcpy(mapped_buffer + perframe_buffersize * i, data_start, perframe_buffersize);
}

uint heapdesc_incrementsize(D3D12_DESCRIPTOR_HEAP_TYPE heaptype)
{
    auto device = globalresources::get().device();
    return static_cast<uint>(device->GetDescriptorHandleIncrementSize(heaptype));
}

uint srvcbvuav_descincrementsize()
{
    return heapdesc_incrementsize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

ComPtr<ID3D12DescriptorHeap> createdescriptorheap(uint maxnumdesc, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    auto device = globalresources::get().device();

    D3D12_DESCRIPTOR_HEAP_DESC heapdesc = {};
    heapdesc.NumDescriptors = UINT(maxnumdesc);
    heapdesc.Type = type;
    heapdesc.Flags = (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV || type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) ? D3D12_DESCRIPTOR_HEAP_FLAG_NONE : D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

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

ComPtr<ID3D12Resource> createtexture_default(CD3DX12_RESOURCE_DESC const& texdesc, stdx::vec4 clear, D3D12_RESOURCE_STATES state)
{
    D3D12_CLEAR_VALUE clearcolour = {};
    clearcolour.Format = texdesc.Format;
    clearcolour.Color[0] = clear[0];
    clearcolour.Color[1] = clear[1];
    clearcolour.Color[2] = clear[2];
    clearcolour.Color[3] = clear[3];

    auto device = globalresources::get().device();
    ComPtr<ID3D12Resource> texdefault;
    auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &texdesc, state, &clearcolour, IID_PPV_ARGS(texdefault.ReleaseAndGetAddressOf())));
    return texdefault;
}

ComPtr<ID3D12Resource> createtexture_default(uint width, uint height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state)
{
    // all unordered access on all textures(on some architecutres unordered access might lead to suboptimal texture layout, but its 2025)
    return createtexture_default(CD3DX12_RESOURCE_DESC::Tex2D(format, static_cast<UINT64>(width), static_cast<UINT>(height), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), {}, state);
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

samplerv samplerheap::addsampler(D3D12_SAMPLER_DESC samplerdesc)
{
    stdx::cassert(currslot < globalresources::max_samplerdescriptors);
    auto device = globalresources::get().device();
    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(d3dheap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(currslot * heapdesc_incrementsize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)));
    device->CreateSampler(&samplerdesc, deschandle);

    return { currslot++, samplerdesc };
}

srv resourceheap::addsrv(D3D12_SHADER_RESOURCE_VIEW_DESC viewdesc, ID3D12Resource* res)
{
    stdx::cassert(currslot < globalresources::max_resdescriptors);
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(d3dheap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(currslot * srvcbvuav_descincrementsize()));
    device->CreateShaderResourceView(res, &viewdesc, deschandle);
    return { currslot++, viewdesc };
}

uav resourceheap::adduav(D3D12_UNORDERED_ACCESS_VIEW_DESC viewdesc, ID3D12Resource* res)
{
    stdx::cassert(currslot < globalresources::max_resdescriptors);
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(d3dheap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(currslot * srvcbvuav_descincrementsize()));
    device->CreateUnorderedAccessView(res, nullptr, &viewdesc, deschandle);
    return { currslot++, viewdesc };
}

uint32 resourceheap::popdesc()
{
    return currslot > 0 ? currslot-- : 0;
}

rtv rtheap::addrtv(ID3D12Resource* res)
{
    stdx::cassert(currslot < globalresources::max_rtdescriptors);
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(d3dheap->GetCPUDescriptorHandleForHeapStart(), INT(currslot * heapdesc_incrementsize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)));
    device->CreateRenderTargetView(res, nullptr, deschandle);
    return { currslot++ };
}

dtv dtheap::adddtv(ID3D12Resource* res)
{
    stdx::cassert(currslot < globalresources::max_dtdescriptors);
    auto device = globalresources::get().device();

    CD3DX12_CPU_DESCRIPTOR_HANDLE deschandle(d3dheap->GetCPUDescriptorHandleForHeapStart(), INT(currslot * heapdesc_incrementsize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)));
    device->CreateDepthStencilView(res, nullptr, deschandle);
    return { currslot++ };
}


srv texturebase::createsrv(DXGI_FORMAT format) const
{
    stdx::cassert(this->d3dresource != nullptr && format != DXGI_FORMAT::DXGI_FORMAT_UNKNOWN);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
    srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvdesc.Format = format;
    srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvdesc.Texture2D.MipLevels = d3dresource->GetDesc().MipLevels;

    return globalresources::get().resourceheap().addsrv(srvdesc, d3dresource.Get());
}

srv texturebase::createsrv(uint32 miplevels, uint32 topmip) const
{
    stdx::cassert(this->d3dresource != nullptr && format != DXGI_FORMAT::DXGI_FORMAT_UNKNOWN);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
    srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvdesc.Format = format;
    srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvdesc.Texture2D.MipLevels = miplevels == 0 ? d3dresource->GetDesc().MipLevels : miplevels;
    srvdesc.Texture2D.MostDetailedMip = topmip;

    return globalresources::get().resourceheap().addsrv(srvdesc, d3dresource.Get());
}

uav texturebase::createuav(uint32 mipslice) const
{
    stdx::cassert(this->d3dresource != nullptr);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
    uavdesc.Format = format;
    uavdesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavdesc.Texture2D.MipSlice = mipslice;

    return globalresources::get().resourceheap().adduav(uavdesc, d3dresource.Get());
}

rtv texturebase::creatertv(rtheap& heap) const
{
    return heap.addrtv(d3dresource.Get());
}

dtv texturebase::createdtv(dtheap& heap) const
{
    return heap.adddtv(d3dresource.Get());
}

void gfx::texture<accesstype::gpu>::create(DXGI_FORMAT dxgiformat, stdx::vecui2 size, D3D12_RESOURCE_STATES state)
{
    stdx::cassert(this->d3dresource == nullptr);

    format = dxgiformat;
    dims = size;
    d3dresource = createtexture_default(dims[0], dims[1], format, state);
}

void texture<accesstype::gpu>::create(CD3DX12_RESOURCE_DESC const& texdesc, stdx::vec4 clear, D3D12_RESOURCE_STATES state)
{
    stdx::cassert(this->d3dresource == nullptr);
    format = texdesc.Format;
    dims = { uint32(texdesc.Width), uint32(texdesc.Height) };
    d3dresource = createtexture_default(texdesc, clear, state);
}

void texture<accesstype::gpu>::createfromfile(std::string const& path)
{
    auto& globalres = gfx::globalresources::get();
    auto device = globalres.device();
    auto cmdlist = globalres.cmdlist();

    stdx::cassert(this->d3dresource == nullptr);

    D3D12_SUBRESOURCE_DATA subresdata;
    ID3D12Resource* texture = nullptr;
    std::unique_ptr<uint8_t[]> wicdata;

    // WIC_LOADER_MIP_AUTOGEN will reserve mips but won't generate them
    DirectX::LoadWICTextureFromFileEx(device.Get(), utils::strtowstr(path).c_str(), 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, WIC_LOADER_MIP_AUTOGEN, &texture, wicdata, subresdata);
    stdx::cassert(texture != nullptr);

    auto const& texdesc = texture->GetDesc();
    format = texture->GetDesc().Format;
    dims = { texture->GetDesc().Width, texture->GetDesc().Height };
    d3dresource = texture;

    const UINT64 ubsize = GetRequiredIntermediateSize(texture, 0, 1);
    auto ub_desc = CD3DX12_RESOURCE_DESC::Buffer(ubsize);

    ComPtr<ID3D12Resource> t_upload;
    auto ut_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, UINT64(dims[0]), UINT(dims[1]));
    auto uploadheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(&uploadheap_desc, D3D12_HEAP_FLAG_NONE, &ub_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadtex.GetAddressOf())));

    // copy texture data to resource
    UpdateSubresources<1>(cmdlist.Get(), texture, uploadtex.Get(), 0, 0, 1, &subresdata);

    auto resource_transition = CD3DX12_RESOURCE_BARRIER::Transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdlist->ResourceBarrier(1, &resource_transition);

    auto const& pipelineobjects = globalres.psomap().find("genmipmaps")->second;

    std::vector<uint32> paramindices(texdesc.MipLevels - 1);
        
    genmipsparams params;
    params.linearsampler = 1;
    for (auto i : stdx::range(texdesc.MipLevels - 1))
    {
        uint32 destdims[2] = { uint32(texdesc.Width >> (i + 1)), uint32(texdesc.Height >> (i + 1)) };

        params.srctexture = createsrv(1, uint32(i)).heapidx;
        params.desttexture = createuav(uint32(i + 1)).heapidx;
        params.texelsize = { 1.0f / float(destdims[0]), 1.0f / float(destdims[1]) };

        mipgenparambuffers.emplace_back().create({ params });

        paramindices[i] = mipgenparambuffers.back().createsrv().heapidx;
    }

    cmdlist->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    ID3D12DescriptorHeap* heaps[] = { globalres.resourceheap().d3dheap.Get(), globalres.samplerheap().d3dheap.Get() };

    cmdlist->SetDescriptorHeaps(_countof(heaps), heaps);
    cmdlist->SetPipelineState(pipelineobjects.pso.Get());

    // todo : move this to engine so we can clean all these temporary descriptors
    for (auto i : stdx::range(texdesc.MipLevels - 1))
    {
        uint32 destdims[2] = { uint32(texdesc.Width >> (i + 1)), uint32(texdesc.Height >> (i + 1)) };

        // only first root constant is read so its fine
        cmdlist->SetComputeRoot32BitConstants(0, 3, &paramindices[i], 0);

        auto dispatchx = UINT(divideup<8>(destdims[0]));
        auto dispatchy = UINT(divideup<8>(destdims[1]));
        cmdlist->Dispatch(dispatchx, dispatchy, 1);

        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(texture);
        cmdlist->ResourceBarrier(1, &uavBarrier);
    }
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

}