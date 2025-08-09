module;

#include <d3d12.h>
#include <wrl.h>
#include "thirdparty/d3dx12.h"

export module graphics:resourcetypes;

import stdxcore;
import stdx;
import std;
import vec;

import graphicscore;

using Microsoft::WRL::ComPtr;

export namespace gfx
{

using default_and_upload_buffers = std::pair<ComPtr<ID3D12Resource>, ComPtr<ID3D12Resource>>;

uint dxgiformatsize(DXGI_FORMAT format);
uint heapdesc_incrementsize(D3D12_DESCRIPTOR_HEAP_TYPE heaptype);
uint srvcbvuav_descincrementsize();

// todo : create buffer structs/classes
ComPtr<ID3D12Resource> create_uploadbuffer(std::byte** mapped_buffer, uint const buffersize);
ComPtr<ID3D12Resource> create_perframeuploadbuffers(std::byte** mapped_buffer, uint const buffersize);
ComPtr<ID3D12Resource> create_uploadbufferwithdata(void const* data_start, uint const buffersize);
ComPtr<ID3D12Resource> create_perframeuploadbufferunmapped(uint const buffersize);
ComPtr<ID3D12DescriptorHeap> createdescriptorheap(uint maxnumdesc, D3D12_DESCRIPTOR_HEAP_TYPE type);
srv createsrv(D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc, ID3D12Resource* resource);
uav createuav(D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc, ID3D12Resource* resource);
void createsrv(D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc, ID3D12Resource* resource, ID3D12DescriptorHeap* resourceheap, uint heapslot = 0);
ComPtr<ID3D12Resource> create_default_uavbuffer(std::size_t const b_size);
ComPtr<ID3D12Resource> create_accelerationstructbuffer(std::size_t const b_size);
default_and_upload_buffers create_defaultbuffer(void const* datastart, std::size_t const b_size);
ComPtr<ID3D12Resource> createtexture_default(CD3DX12_RESOURCE_DESC const& texdesc, stdx::vec4 clear, D3D12_RESOURCE_STATES state);
ComPtr<ID3D12Resource> createtexture_default(uint width, uint height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state);
void update_buffer(std::byte* mapped_buffer, void const* data_start, std::size_t const data_size);
uint updatesubres(ID3D12Resource* dest, ID3D12Resource* upload, D3D12_SUBRESOURCE_DATA const* srcdata);
D3D12_GPU_VIRTUAL_ADDRESS get_perframe_gpuaddress(D3D12_GPU_VIRTUAL_ADDRESS start, UINT64 perframe_buffersize);
void update_currframebuffer(std::byte* mapped_buffer, void const* data_start, std::size_t const data_size, std::size_t const perframe_buffersize);
void update_allframebuffers(std::byte* mapped_buffer, void const* data_start, uint const perframe_buffersize);
uint align(uint unalignedsize, uint alignment) { return stdx::nextpowoftwomultiple(unalignedsize, alignment); }

struct resource
{
	D3D12_GPU_VIRTUAL_ADDRESS gpuaddress() const { return d3dresource->GetGPUVirtualAddress(); }
	[[nodiscard]] bool valid() const { return d3dresource != nullptr; }

	uint ressize = 0;
	ComPtr<ID3D12Resource> d3dresource;
};

template<D3D12_DESCRIPTOR_HEAP_TYPE heaptype, uint32 maxdescs>
class heap
{
public:

	static constexpr uint32 maxdescriptors = maxdescs;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuhandle(uint32 slot) const
	{
		return { d3dheap->GetCPUDescriptorHandleForHeapStart(), INT(slot * heapdesc_incrementsize(heaptype)) };
	}

	uint32 currslot = 0;
	ComPtr<ID3D12DescriptorHeap> d3dheap;
};

class samplerheap : public heap<D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048>
{
public:
	samplerv addsampler(D3D12_SAMPLER_DESC samplerdesc);
};

class resourceheap : public heap<D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 10000>
{
public:
	srv addsrv(D3D12_SHADER_RESOURCE_VIEW_DESC view, ID3D12Resource* res);
	uav adduav(D3D12_UNORDERED_ACCESS_VIEW_DESC view, ID3D12Resource* res);

	uint32 popdesc();
};

class rtheap : public heap<D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 100>
{
public:
	rtv addrtv(ID3D12Resource* res);
};

class dtheap : public heap<D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 100>
{
public:
	dtv adddtv(ID3D12Resource* res);
};

export enum class accesstype
{
	both,		// cpu w, gpu rw
	gpu,		// gpu rw
};

template<typename t>
struct structuredbufferbase : public resource
{
	srv createsrv()
	{
		stdx::cassert(d3dresource != nullptr);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
		srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvdesc.Format = DXGI_FORMAT_UNKNOWN;
		srvdesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvdesc.Buffer.NumElements = UINT(numelements);
		srvdesc.Buffer.StructureByteStride = UINT(sizeof(t));

		return gfx::createsrv(srvdesc, d3dresource.Get());
	}

	uav createuav()
	{
		stdx::cassert(d3dresource != nullptr);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
		uavdesc.Format = DXGI_FORMAT_UNKNOWN;
		uavdesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavdesc.Buffer.NumElements = UINT(numelements);
		uavdesc.Buffer.StructureByteStride = UINT(sizeof(t));
		return gfx::createuav(uavdesc, d3dresource.Get());
	}

	uint numelements = 0;
};

template<typename t, accesstype access>
struct structuredbuffer : public structuredbufferbase<t> {};

// cpu writeable
template<typename t>
struct structuredbuffer<t, accesstype::both> : public structuredbufferbase<t>
{
	// create without intitial data
	void create(uint32 numelements = 1)
	{
		stdx::cassert(this->d3dresource == nullptr);

		UINT buffersize = UINT(numelements * sizeof(t));
		ComPtr<ID3D12Resource> uploadbuffer = create_uploadbuffer(&_mappeddata, buffersize);
		this->d3dresource = uploadbuffer;
		this->numelements = numelements;
	}

	void create(std::vector<t> const& data)
	{
		stdx::cassert(this->d3dresource == nullptr);

		UINT buffersize = UINT(data.size() * sizeof(t));
		ComPtr<ID3D12Resource> uploadbuffer = create_uploadbuffer(&_mappeddata, buffersize);
		memcpy(_mappeddata, data.data(), buffersize);

		this->d3dresource = uploadbuffer;
		this->numelements = data.size();
	}

	void update(std::vector<t> const& data)
	{
		stdx::cassert(this->d3dresource != nullptr);
		stdx::cassert(data.size() <= this->numelements, "cannot update buffer with more data than initial size");

		size_t updatesize = data.size() * sizeof(t);
		memcpy(_mappeddata, data.data(), updatesize);
	}

	std::byte* _mappeddata = nullptr;
};

// gpu only
template<typename t>
struct structuredbuffer<t, accesstype::gpu> : public structuredbufferbase<t>
{
	void create(uint numelems)
	{
		stdx::cassert(this->d3dresource == nullptr);

		this->d3dresource = create_default_uavbuffer(sizeof(t) * numelems);
		this->numelements = numelems;
	}
};

struct texturebase : public resource
{
	srv createsrv(DXGI_FORMAT format) const;
	srv createsrv(uint32 miplevels = 0, uint32 topmip = 0) const;
	uav createuav(uint32 mipslice = 0) const;
	rtv creatertv(rtheap& heap) const;
	dtv createdtv(dtheap& heap) const;

	stdx::vecui2 dims;
	DXGI_FORMAT format;

	struct genmipsparams
	{
		uint32 srctexture;
		uint32 desttexture;
		uint32 linearsampler;
		stdx::vec2 texelsize;
	};

	// todo : shouldn't be needed if done in engine
	std::vector<structuredbuffer<genmipsparams, accesstype::both>> mipgenparambuffers;
};

template<accesstype access>
struct texture : public texturebase {};

// gpu rw, cpu can only write initially
template<>
struct texture<accesstype::gpu> : public texturebase
{
	void create(DXGI_FORMAT dxgiformat, stdx::vecui2 size, D3D12_RESOURCE_STATES state);
	void create(CD3DX12_RESOURCE_DESC const& texdesc, stdx::vec4 clear, D3D12_RESOURCE_STATES state);
	void createfromfile(std::string const& path);

	// unlike buffers texture are created on default heap?
	ComPtr<ID3D12Resource> uploadtex;
};

template<typename t>
struct dynamicbuffer
{
	void createresource(uint maxcount)
	{
		_maxcount = maxcount;
		_buffer = create_perframeuploadbuffers(&_mappeddata, buffersize());
	}

	void updateresource(std::vector<t> const& data)
	{
		_count = data.size();
		stdx::cassert(_count <= _maxcount);
		update_currframebuffer(_mappeddata, data.data(), datasize(), buffersize());
	}

	uint count() const { return _count; }
	uint buffersize() const { return _maxcount * sizeof(t); }
	uint datasize() const { return _count * sizeof(t); }

	D3D12_GPU_VIRTUAL_ADDRESS gpuaddress() const { return get_perframe_gpuaddress(_buffer->GetGPUVirtualAddress(), buffersize()); }

	uint _maxcount = 0;
	uint _count = 0;
	std::byte* _mappeddata = nullptr;
	ComPtr<ID3D12Resource> _buffer;
};

// todo : this is a texture updated per frame,
// update to be more consistent with texture
struct texture_dynamic
{
	void createresource(stdx::vecui2 dims, std::vector<uint8_t> const& texturedata);
	void updateresource(std::vector<uint8_t> const& texturedata);
	D3D12_GPU_DESCRIPTOR_HANDLE deschandle() const;

	uint size() const;

	DXGI_FORMAT _format;
	stdx::vecui2 _dims;
	srv _srv;
	ComPtr<ID3D12Resource> _texture;
	ComPtr<ID3D12Resource> _bufferupload;
};

}
