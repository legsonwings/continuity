module;

#include <wrl.h>
#include <d3d12.h>
#include "thirdparty/d3dx12.h"

// todo : find a place for shared constants
#include "shared/sharedconstants.h"
#include "simplemath/simplemath.h"

export module graphics;
export import :model;
export import :texture;
export import :resourcetypes;
export import :globalresources;

import stdxcore;
import stdx;
import vec;
import std;

// todo : make graphicscore partial module unit
import graphicscore;
import geometry;

using Microsoft::WRL::ComPtr;

export namespace gfx
{

// helpers
std::string generaterandom_matcolor(stdx::ext<material, bool> definition, std::optional<std::string> const& preferred_name = {});

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

// raytrace stuff below

using rtvertexbuffer = structuredbuffer<stdx::vec3, accesstype::both>;
using rtindexbuffer = structuredbuffer<uint32_t, accesstype::both>;

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

}