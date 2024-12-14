module;

#include <wrl.h>
#include <d3d12.h>
#include "thirdparty/d3dx12.h"
#include <atlbase.h>
#include <atlconv.h>

// create a module gfx core
// todo : find a place for shared constants
#include "sharedconstants.h"
#include "simplemath/simplemath.h"

export module graphics;

import stdxcore;
import stdx;
import vec;
import std.core;

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

export namespace gfx
{

using namespace DirectX;
 
using vector2 = DirectX::SimpleMath::Vector2;
using vector3 = DirectX::SimpleMath::Vector3;
using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;

using resourcelist = std::vector<ComPtr<ID3D12Resource>>;

enum class topology
{
    triangle,
    line,
};

enum psoflags
{
    none = 0x0,
    wireframe = 0x1,
    transparent = 0x2,
    twosided = 0x4
};

struct vertex
{
	vector3 position = {};
	vector3 normal = {};
	vector2 texcoord = {};
};

struct color
{
    static inline vector4 red = vector4{ 1.f, 0.f, 0.f, 1.f };
    static inline vector4 black = vector4{ 0.f, 0.f, 0.f, 1.f };
    static inline vector4 white = vector4{ 1.f, 1.f, 1.f, 1.f };
	static inline vector4 water = vector4{ 0.2f, 0.4f, 0.6f, 0.7f };
};

struct shader
{
    byte* data;
    uint32_t size;
};

struct renderparams
{
    bool wireframe = false;
};

struct buffer
{
    uint32_t slot;
    D3D12_GPU_DESCRIPTOR_HANDLE deschandle = { 0 };
};

struct rootbuffer
{
    uint32_t slot;
    D3D12_GPU_VIRTUAL_ADDRESS address = 0;
};

struct rootconstants
{
    uint32_t slot;
    std::vector<uint8_t> values;
};

struct pipeline_objects
{
    ComPtr<ID3D12PipelineState> pso;
    ComPtr<ID3D12PipelineState> pso_wireframe;
    ComPtr<ID3D12PipelineState> pso_twosided;
	ComPtr<ID3D12StateObject>	pso_raytracing;
    ComPtr<ID3D12RootSignature> root_signature;
};

struct resource_bindings
{
    rootbuffer constant;
    rootbuffer objectconstant;
    rootbuffer vertex;
    rootbuffer instance;
	rootbuffer customdata;
    buffer texture;
    rootconstants rootconstants;
    pipeline_objects pipelineobjs;
};

struct viewinfo
{
    matrix view;
    matrix proj;
};

// these are passed to gpu, so be careful about alignment and padding
struct material
{
    // the member order is deliberate
    vector3 fr = { 0.01f, 0.01f, 0.01f };
    float r = 0.25f;
    vector4 a = vector4::One;

    material& roughness(float _r) { r = _r; return *this; }
    material& diffuse(vector4 const& _a) { a = _a; return *this; }
    material& fresnelr(vector3 const& _fr) { fr = _fr; return *this; }
};

// this is used in constant buffer so alignment is important
struct instance_data
{
    matrix matx;
    matrix normalmatx;
    matrix mvpmatx;
    material mat;
    instance_data() = default;
    instance_data(matrix const& m, viewinfo const& v, material const& _material)
        : matx(m.Transpose()), normalmatx(m.Invert()), mvpmatx((m * v.view * v.proj).Transpose()), mat(_material) {}
};

struct alignas(256) objectconstants : public instance_data
{
    objectconstants() = default;
    objectconstants(matrix const& m, viewinfo const& v, material const& _material) : instance_data(m, v, _material) {}
};

struct light
{
    vector3 color;
    float range;
    vector3 position;
    uint8_t padding1[4];
    vector3 direction;
    uint8_t padding2[4];
};

struct alignas(256) sceneconstants
{
    vector3 campos;
    uint8_t padding[4];
    vector4 ambient;
    light lights[MAX_NUM_LIGHTS];
	matrix viewproj;
    uint32_t numdirlights = 0;
    uint32_t numpointlights;
};

struct resource;

// gfx core

using psomapref = std::unordered_map<std::string, pipeline_objects> const&;
using matmapref = std::unordered_map<std::string, stdx::ext<material, bool>> const&;
using materialentry = stdx::ext<material, bool>;
using materialcref = stdx::ext<material, bool> const&;

// helpers
std::string generaterandom_matcolor(stdx::ext<material, bool> definition, std::optional<std::string> const& preferred_name = {});

using default_and_upload_buffers = std::pair<ComPtr<ID3D12Resource>, ComPtr<ID3D12Resource>>;

uint dxgiformatsize(DXGI_FORMAT format);
uint srvsbvuav_descincrementsize();
ComPtr<ID3D12Resource> create_uploadbuffer(std::byte** mapped_buffer, uint const buffersize);
ComPtr<ID3D12Resource> create_uploadbufferunmapped(uint const buffersize);
ComPtr<ID3D12DescriptorHeap> createsrvdescriptorheap(D3D12_DESCRIPTOR_HEAP_DESC heapdesc);
void createsrv(D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc, ID3D12Resource* resource, ID3D12DescriptorHeap* srvheap, uint heapslot = 0);
ComPtr<ID3D12Resource> create_default_uavbuffer(std::size_t const b_size);
default_and_upload_buffers create_defaultbuffer(void const* datastart, std::size_t const b_size);
ComPtr<ID3D12Resource> createtexture_default(uint width, uint height, DXGI_FORMAT format);
uint updatesubres(ID3D12Resource* dest, ID3D12Resource* upload, D3D12_SUBRESOURCE_DATA const* srcdata);
D3D12_GPU_VIRTUAL_ADDRESS get_perframe_gpuaddress(D3D12_GPU_VIRTUAL_ADDRESS start, UINT64 perframe_buffersize);
void update_perframebuffer(std::byte* mapped_buffer, void const* data_start, std::size_t const data_size, std::size_t const perframe_buffersize);
void update_allframebuffers(std::byte* mapped_buffer, void const* data_start, uint const perframe_buffersize);

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

struct cballocationhelper
{
	static constexpr unsigned cbalignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
	static alignedlinearallocator& allocator() { static alignedlinearallocator allocator{ cbalignment }; return allocator; }
};

template<typename t>
requires (sizeof(t) % 256 == 0 && stdx::triviallycopyable_c<t>)
struct constantbuffer
{
	void createresource()
	{
		_data = cballocationhelper::allocator().allocate<t>();
		_buffer = create_uploadbuffer(&_mappeddata, size());
	}

	t& data() const { return *_data; }
	void updateresource() { update_perframebuffer(_mappeddata, _data, size(), size()); }

	template<typename u>
	requires std::same_as<t, std::decay_t<u>>
	void updateresource(u&& data)
	{
		*_data = data;
		update_perframebuffer(_mappeddata, _data, size(), size());
	}

	uint size() const { return sizeof(t); }
	D3D12_GPU_VIRTUAL_ADDRESS gpuaddress() const { return get_perframe_gpuaddress(_buffer->GetGPUVirtualAddress(), size()); }

	t* _data = nullptr;
	std::byte* _mappeddata = nullptr;
	ComPtr<ID3D12Resource> _buffer;
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

// todo : make other classes use this
struct resource
{
	D3D12_GPU_VIRTUAL_ADDRESS gpuaddress() const { return d3dresource->GetGPUVirtualAddress(); }

	uint ressize = 0;
	ComPtr<ID3D12Resource> d3dresource;
};

// todo : specify type using a template
struct structuredbuffer : public resource
{
	void createresources(std::string name, uint buffersize)
	{
		ressize = buffersize;
		d3dresource = create_default_uavbuffer(buffersize);
		stdx::cassert(d3dresource != nullptr);
		d3dresource->SetName(CA2W(name.c_str()));
	}
};

template<typename t>
struct dynamicbuffer
{
	void createresource(uint maxcount)
	{
		_maxcount = maxcount;
		_buffer = create_uploadbuffer(&_mappeddata, buffersize());
	}

	void updateresource(std::vector<t> const& data)
	{
		_count = data.size();
		stdx::cassert(_count <= _maxcount);
		update_perframebuffer(_mappeddata, data.data(), datasize(), buffersize());
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

struct texture
{
	void createresource(uint heapidx, stdx::vecui2 dims, std::vector<uint8_t> const& texturedata, ID3D12DescriptorHeap* srvheap);
	void updateresource(std::vector<uint8_t> const& texturedata);
	D3D12_GPU_DESCRIPTOR_HANDLE deschandle() const;

	uint size() const;

	uint _heapidx;
	DXGI_FORMAT _format;
	stdx::vecui2 _dims;
	ComPtr<ID3D12Resource> _texture;
	ComPtr<ID3D12Resource> _bufferupload;
	ComPtr<ID3D12DescriptorHeap> _srvheap;
};

// todo : theres partial module implementations?? these should be split better
class globalresources
{
	viewinfo _view;
	uint _frameindex{ 0 };
	std::string _assetspath;
	constantbuffer<sceneconstants> _cbuffer;
	ComPtr<ID3D12DescriptorHeap> _srvheap;
	ComPtr<ID3D12Device5> _device;
	std::vector<ComPtr<ID3D12RootSignature>> _rootsig;
	ComPtr<ID3D12GraphicsCommandList6> _commandlist;
	std::unordered_map<std::string, pipeline_objects> _psos;
	std::unordered_map<std::string, stdx::ext<material, bool>> _materials;
	stdx::ext<material, bool> _defaultmat{ {}, false };
	std::unordered_map<DXGI_FORMAT, uint> _dxgisizes{ {DXGI_FORMAT_R8G8B8A8_UNORM, 4} };
	D3DX12_MESH_SHADER_PIPELINE_STATE_DESC _psodesc{};

	std::string assetfullpath(std::string const& path) const;
public:
	void init();

	viewinfo& view();
	psomapref psomap() const;
	matmapref matmap() const;
	materialcref defaultmat() const;
	constantbuffer<sceneconstants>& cbuffer();
	ComPtr<ID3D12Device5>& device();
	ComPtr<ID3D12DescriptorHeap>& srvheap();
	ComPtr<ID3D12GraphicsCommandList6>& cmdlist();
	void frameindex(uint idx);
	uint frameindex() const;
	uint dxgisize(DXGI_FORMAT format);
	materialcref mat(std::string const& name);
	void psodesc(D3DX12_MESH_SHADER_PIPELINE_STATE_DESC const& psodesc);
	materialcref addmat(std::string const& name, material const& mat, bool twosided = false);
	void addcomputepso(std::string const& name, std::string const& cs);
	void addpso(std::string const& name, std::string const& as, std::string const& ms, std::string const& ps, uint flags = psoflags::none);
	void addraytracingpso(std::string const& libname);

	static globalresources& get();
};

}