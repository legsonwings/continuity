module;

#include <wrl.h>
#include <d3d12.h>
#include "thirdparty/d3dx12.h"

// create a module gfx core
// todo : find a place for shared constants
#include "shared/sharedconstants.h"
#include "simplemath/simplemath.h"

export module graphics;

import stdxcore;
import stdx;
import vec;
import std;

import graphicscore;
import geometry;

using Microsoft::WRL::ComPtr;

export struct alignedlinearallocator
{
	alignedlinearallocator(uint alignment);

	template<typename t>
	requires std::default_initializable<t>
	t* allocate()
	{
		uint const alignedsize = stdx::nextpowoftwomultiple(sizeof(t), _alignment);

		stdx::cassert(_currentpos != nullptr);
		stdx::cassert(canallocate(alignedsize));
		t* r = reinterpret_cast<t*>(_currentpos);
		stdx::cassert(r != nullptr);
		*r = t{};
		_currentpos += alignedsize;
		return r;
	}
private:

	bool canallocate(uint size) const;

	static constexpr uint buffersize = 51200;
	std::byte _buffer[buffersize];
	std::byte* _currentpos = &_buffer[0];
	uint _alignment;
};

struct cballocationhelper
{
	static constexpr unsigned cbalignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	static alignedlinearallocator& allocator() { static alignedlinearallocator allocator{ cbalignment }; return allocator; }
};

export namespace gfx
{

struct resource
{
	D3D12_GPU_VIRTUAL_ADDRESS gpuaddress() const { return d3dresource->GetGPUVirtualAddress(); }

	uint ressize = 0;
	ComPtr<ID3D12Resource> d3dresource;
};

// all resource views are created on the global resource heap right now
struct resourceview
{
	uint32 heapidx;
};

struct srv : public resourceview
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc;
};

struct uav : public resourceview
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
};

struct cbv : public resourceview
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
};


// gfx core

using psomapref = std::unordered_map<std::string, pipeline_objects> const&;
using matmapref = std::unordered_map<std::string, stdx::ext<material, bool>> const&;
using materialentry = stdx::ext<material, bool>;
using materialcref = stdx::ext<material, bool> const&;

// helpers
std::string generaterandom_matcolor(stdx::ext<material, bool> definition, std::optional<std::string> const& preferred_name = {});

using default_and_upload_buffers = std::pair<ComPtr<ID3D12Resource>, ComPtr<ID3D12Resource>>;

uint dxgiformatsize(DXGI_FORMAT format);
uint srvcbvuav_descincrementsize();

// todo : create buffer structs/classes
ComPtr<ID3D12Resource> create_uploadbuffer(std::byte** mapped_buffer, uint const buffersize);
ComPtr<ID3D12Resource> create_perframeuploadbuffers(std::byte** mapped_buffer, uint const buffersize);
ComPtr<ID3D12Resource> create_uploadbufferwithdata(void const* data_start, uint const buffersize);
ComPtr<ID3D12Resource> create_perframeuploadbufferunmapped(uint const buffersize);
ComPtr<ID3D12DescriptorHeap> createresourcedescriptorheap();
srv createsrv(D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc, ID3D12Resource* resource);
uav createuav(D3D12_UNORDERED_ACCESS_VIEW_DESC uavdesc, ID3D12Resource* resource);
void createsrv(D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc, ID3D12Resource* resource, ID3D12DescriptorHeap* resourceheap, uint heapslot = 0);
ComPtr<ID3D12Resource> create_default_uavbuffer(std::size_t const b_size);
ComPtr<ID3D12Resource> create_accelerationstructbuffer(std::size_t const b_size);
default_and_upload_buffers create_defaultbuffer(void const* datastart, std::size_t const b_size);
ComPtr<ID3D12Resource> createtexture_default(uint width, uint height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state);
void update_buffer(std::byte* mapped_buffer, void const* data_start, std::size_t const data_size);
uint updatesubres(ID3D12Resource* dest, ID3D12Resource* upload, D3D12_SUBRESOURCE_DATA const* srcdata);
D3D12_GPU_VIRTUAL_ADDRESS get_perframe_gpuaddress(D3D12_GPU_VIRTUAL_ADDRESS start, UINT64 perframe_buffersize);
void update_currframebuffer(std::byte* mapped_buffer, void const* data_start, std::size_t const data_size, std::size_t const perframe_buffersize);
void update_allframebuffers(std::byte* mapped_buffer, void const* data_start, uint const perframe_buffersize);
cbv createcbv(uint size, ID3D12Resource* res);

template<typename... args>
void uav_barrier(ComPtr<ID3D12GraphicsCommandList6>& cmdlist, args const&... resources)
{
	constexpr int num_var_args = sizeof ... (args);
	static_assert(num_var_args > 0);

	// could have done this using tuple, but unfortunately tuple::get<i> is not constexpr so we need to allocate an array
	// maybe use a custom tuple?
	// auto tuple = std::tie(resources...);
	resource resources_array[num_var_args] = { resources... };

	std::array<CD3DX12_RESOURCE_BARRIER, num_var_args> barriers;
	for (auto i : stdx::range(num_var_args))
		barriers[i] = CD3DX12_RESOURCE_BARRIER::UAV(resources_array[i].d3dresource.Get());

	cmdlist->ResourceBarrier((UINT)barriers.size(), barriers.data());
}

// helpers

enum class accesstype
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

using rtvertexbuffer = structuredbuffer<stdx::vec3, accesstype::both>;
using rtindexbuffer = structuredbuffer<uint32_t, accesstype::both>;

class resourceheap
{
public:
	srv addsrv(D3D12_SHADER_RESOURCE_VIEW_DESC view, ID3D12Resource* res);
	uav adduav(D3D12_UNORDERED_ACCESS_VIEW_DESC view, ID3D12Resource* res);
	cbv addcbv(D3D12_CONSTANT_BUFFER_VIEW_DESC view, ID3D12Resource* res);

	D3D12_GPU_DESCRIPTOR_HANDLE gpudeschandle(uint slot) const;

	ComPtr<ID3D12DescriptorHeap> d3dheap;
private:
	uint32 currslot = 0;
};

// read write by gpu only
struct texture : public resource
{
	texture() = default;
	texture(DXGI_FORMAT dxgiformat, stdx::vecui2 dimensions, D3D12_RESOURCE_STATES state);

	srv createsrv() const;
	uav createuav() const;

	DXGI_FORMAT format;
	stdx::vecui2 dims;
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

struct model
{
	model() = default;
	model(std::string const& objpath, bool translatetoorigin = false);

	std::vector<vertex> const& vertices() const { return _vertices; }
	std::vector<uint32> const& indices() const { return _indices; }

	// center at origin for now
	std::vector<instance_data> instancedata() const;

	std::vector<uint32> _indices;
	std::vector<vertex> _vertices;
};

uint align(uint unalignedsize, uint alignment) { return stdx::nextpowoftwomultiple(unalignedsize, alignment); }
uint alignshaderrecord(uint unalignedsize) { return align(unalignedsize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT); }

template<typename rootargs_t>
struct shaderrecord
{
	static constexpr uint unalignedsize = sizeof(rootargs_t) + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	static constexpr uint alignedsize = alignshaderrecord(unalignedsize);
};

template<>
struct shaderrecord<void>
{
	static constexpr uint unalignedsize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	static constexpr uint alignedsize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
};

using shaderrecord_default = shaderrecord<void>;

template<typename... rootargs_ts>
struct shadertable_recordsize
{
	static constexpr uint size = std::max<uint>({ shaderrecord<rootargs_ts>::alignedsize... });
};

class shadertable : public resource
{

public:
	shadertable() = default;
	shadertable(uint recordsize, uint numrecords);

	uint recordsize() const { return shaderrecordsize; }
	uint numrecords() const { return numshaderrecords; }

	// default shader record
	void addrecord(void* shaderid)
	{
		stdx::cassert(shaderrecord_default::alignedsize <= shaderrecordsize);

		std::byte* recordstart = getnewrecord_start();
		memcpy(recordstart, shaderid, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

		numshaderrecordswritten++;
	}

	template<typename rootargs_t>
	void addrecord(void* shaderid, rootargs_t const& rootargs) requires !std::is_same_v<rootargs_t, void>
	{
		stdx::cassert(shaderrecord<rootargs_t>::alignedsize <= shaderrecordsize);

		std::byte* recordstart = getnewrecord_start();
		std::memcpy(recordstart, shaderid, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		recordstart = recordstart + shaderrecordsize;
		std::memcpy(recordstart, &rootargs, sizeof(rootargs_t));

		numshaderrecordswritten += 2;
	}

	std::byte* getnewrecord_start() const { return mapped_records + numshaderrecordswritten * shaderrecordsize; }

private:

	uint shaderrecordsize = 0;
	uint numshaderrecords = 0;
	uint numshaderrecordswritten = 0;
	std::byte* mapped_records = nullptr;
};

enum class blastype
{
	triangle,
	procedural
};

enum class geometryopacity
{
	opaque,
	transparent
};

struct accelerationstruct : public resource { };

struct blas : public accelerationstruct
{
	// return scratch resource
	ComPtr<ID3D12Resource> kickoffbuild(D3D12_RAYTRACING_GEOMETRY_DESC const& geometrydesc);
};

struct blasinstancedescs
{
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> descs;
};

struct tlas : public accelerationstruct
{
	// number of resources we need to keep alive until gpu is done with acceleration structure build
	static constexpr uint numresourcetokeepalive = 2;
	std::array<ComPtr<ID3D12Resource>, numresourcetokeepalive> build(blasinstancedescs const& instancedescs);

	srv createsrv();
};

// blases only support single primitve instance right now
struct triblas : public blas
{
	static constexpr uint numresourcetokeepalive = 1;
	std::array<ComPtr<ID3D12Resource>, numresourcetokeepalive> build(blasinstancedescs& instancedescs, geometryopacity opacity, rtvertexbuffer const& vertices, rtindexbuffer const& indices);
};

struct proceduralblas : public blas
{
	static constexpr uint numresourcetokeepalive = 2;
	std::array<ComPtr<ID3D12Resource>, numresourcetokeepalive> build(blasinstancedescs& instancedescs, geometryopacity opacity, geometry::aabb const& aabb);
};

struct raytrace
{
	void dispatchrays(shadertable const& raygen, shadertable const& miss, shadertable const& hitgroup, ID3D12StateObject* stateobject, uint width, uint height);
	void copyoutputtorendertarget(texture const& rtoutput);
};

template<typename t>
requires (sizeof(t) % 256 == 0 && stdx::triviallycopyable_c<t>)
struct constantbuffer : public resource
{
	void createresource()
	{
		_data = cballocationhelper::allocator().allocate<t>();
		d3dresource = create_perframeuploadbuffers(&_mappeddata, size());
	}

	cbv createcbv() const
	{
		// todo : should this be size of whole resource
		return gfx::createcbv(size(), d3dresource.Get());
	}

	t& data() const { return *_data; }
	void updateresource() { update_currframebuffer(_mappeddata, _data, size(), size()); }

	template<typename u>
	requires std::same_as<t, std::decay_t<u>>
	void updateresource(u&& data)
	{
		*_data = data;
		update_currframebuffer(_mappeddata, _data, size(), size());
	}

	uint size() const { return sizeof(t); }
	D3D12_GPU_VIRTUAL_ADDRESS currframe_gpuaddress() const { return get_perframe_gpuaddress(d3dresource->GetGPUVirtualAddress(), size()); }

	t* _data = nullptr;
	std::byte* _mappeddata = nullptr;

private:
	// todo : hide base class function so we fail on its use
	// todo : replace uses with currframe_gpuaddress()
	D3D12_GPU_VIRTUAL_ADDRESS gpuaddress() const {}
};

template<typename t, uint n>
requires (sizeof(t) % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0 && stdx::triviallycopyable_c<t>)
struct constantbuffer2
{
	void createresource()
	{
		alignedsize = align(size(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		for (uint i(0); i < n; ++i)
		{
			buffers[i] = create_uploadbuffer(&_mappeddata[i], alignedsize);
		}
	}

	std::array<cbv, n> createcbv() const
	{
		std::array<cbv, n> r;
		for (uint i(0); i < n; ++i)
		{
			r[i] = gfx::createcbv(alignedsize, buffers[i].Get());
		}

		return r;
	}

	t& data(uint i) const { return *reinterpret_cast<t*>(_mappeddata[i]); }

	uint size() const { return sizeof(t); }

private:
	uint alignedsize = 0;
	ComPtr<ID3D12Resource> buffers[n];
	std::byte* _mappeddata[n];
};

template<typename t>
struct staticbuffer
{
	ComPtr<ID3D12Resource> createresources(std::vector<t> const& data)
	{
		_count = data.size();
		auto const bufferresources = create_defaultbuffer(data.data(), size());
		_buffer = bufferresources.first;
		return bufferresources.second;
	}

	uint count() const { return _count; }
	uint size() const { return _count * sizeof(t); }
	D3D12_GPU_VIRTUAL_ADDRESS gpuaddress() const { return _buffer->GetGPUVirtualAddress(); }

	uint _count = 0;
	ComPtr<ID3D12Resource> _buffer;
};

struct rostructuredbuffer : public resource
{
	void createresources(uint buffersize)
	{
		ressize = buffersize;
		d3dresource = create_default_uavbuffer(buffersize);
	}
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

// todo : theres partial module implementations?? these should be split better
class globalresources
{
	viewinfo _view;
	uint _frameindex{ 0 };
	std::string _assetspath;
	constantbuffer<sceneconstants> _cbuffer;
	resourceheap _resourceheap;
	ComPtr<ID3D12Device5> _device;

	ComPtr<ID3D12Resource> _rendertarget;
	ComPtr<ID3D12GraphicsCommandList6> _commandlist;
	std::unordered_map<std::string, pipeline_objects> _psos;
	//std::unordered_map<std::string, stdx::ext<material, bool>> _materials;
	stdx::ext<material, bool> _defaultmat{ {}, false };

	std::vector<material> _materials;
	std::unordered_map<DXGI_FORMAT, uint> _dxgisizes{ {DXGI_FORMAT_R8G8B8A8_UNORM, 4} };
	D3DX12_MESH_SHADER_PIPELINE_STATE_DESC _psodesc{};

	uint32 _materialsbuffer_idx = -1;//stdx::invalid<uint32>;
	gfx::structuredbuffer<material, gfx::accesstype::both> materialsbuffer;

	std::string assetfullpath(std::string const& path) const;
public:
	void init();
	void create_resources();

	static constexpr uint32 max_materials = 100;

	viewinfo& view();
	psomapref psomap() const;
	matmapref matmap() const;
	uint32 materialsbuffer_idx() const;
	materialcref defaultmat() const;
	constantbuffer<sceneconstants>& cbuffer();
	void rendertarget(ComPtr<ID3D12Resource>& rendertarget);
	ComPtr<ID3D12Resource>& rendertarget();
	ComPtr<ID3D12Device5>& device();
	resourceheap& resourceheap();
	ComPtr<ID3D12GraphicsCommandList6>& cmdlist();
	void frameindex(uint idx);
	uint frameindex() const;
	uint dxgisize(DXGI_FORMAT format);
	material& mat(uint32 matid);
	void psodesc(D3DX12_MESH_SHADER_PIPELINE_STATE_DESC const& psodesc);
	uint32 addmat(material const& mat);
	void addcomputepso(std::string const& name, std::string const& cs);
	void addpso(std::string const& name, std::string const& as, std::string const& ms, std::string const& ps, uint flags = psoflags::none);
	pipeline_objects& addraytracingpso(std::string const& name, std::string const& libname, raytraceshaders const& shaders);

	static globalresources& get();
};

}