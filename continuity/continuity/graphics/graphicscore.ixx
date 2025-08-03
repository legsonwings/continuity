module;

#include <wrl.h>
#include <d3d12.h>
#include <thirdparty/d3dx12.h>

// create a module gfx core
// todo : find a place for shared constants
#include "shared/sharedconstants.h"
#include "simplemath/simplemath.h"

export module graphicscore;

import stdxcore;
import stdx;
import vec;
import std;

using Microsoft::WRL::ComPtr;

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

struct index
{
    uint32 pos;
    uint32 texcoord;
    uint32 tbn;
};

struct tbn
{
    vector3 normal = {};
    vector3 tangent = {};
    vector3 bitangent = {};
};

struct vertex
{
	vector3 position = {};
    vector2 texcoord = {};
	vector3 normal = {};
    vector3 tangent = {};
    vector3 bitangent = {};
};

struct vertexattribs
{
    std::vector<vector3> positions;
    std::vector<vector2> texcoords;
    std::vector<tbn> tbns;
};

struct color
{
    static constexpr auto red = stdx::vec4{ 1.f, 0.f, 0.f, 1.f };
    static constexpr auto black = stdx::vec4{ 0.f, 0.f, 0.f, 1.f };
    static constexpr auto white = stdx::vec4{ 1.f, 1.f, 1.f, 1.f };
	static constexpr auto water = stdx::vec4{ 0.2f, 0.4f, 0.6f, 0.7f };
};

struct shader
{
    std::byte* data;
    uint32_t size;
};

struct renderparams
{
    std::string psoname;
    bool shadowpass = false;
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
    uint32 slot;
    std::vector<uint32> values;
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

struct samplerv : public resourceview
{
    D3D12_SAMPLER_DESC desc;
};

struct pipeline_objects
{
    ComPtr<ID3D12PipelineState> pso;
    ComPtr<ID3D12PipelineState> pso_wireframe;
    ComPtr<ID3D12PipelineState> pso_twosided;
	ComPtr<ID3D12StateObject>	pso_raytracing;
    ComPtr<ID3D12RootSignature> root_signature;

	// local root signature used in ray tracing, only one is supported right now
	ComPtr<ID3D12RootSignature> rootsignature_local;
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

struct material
{
    stdx::vec4 basecolour = stdx::vec4{ 1, 0, 0, 0 };
    float roughness = 0.25f;
    float reflectance = 0.5f;
    float metallic = 0.0f;

    uint32 diffusetex = stdx::invalid<uint32>;
    uint32 roughnesstex = stdx::invalid<uint32>;
    uint32 normaltex = stdx::invalid<uint32>;

    material& colour(stdx::vec4 const& colour) { basecolour = colour; return *this; }
};

// this is used in constant buffer so alignment is important
struct instance_data
{
    matrix matx;
    matrix normalmatx;
    matrix mvpmatx;
    
    // single material
    uint32 mat;

    // per primitive materials
    // this isn't a per instance thing, but its here for convenience
    uint32 primmaterialsidx;

    instance_data() = default;
    instance_data(matrix const& m, viewinfo const& v)
        : matx(m.Transpose()), normalmatx(m.Invert()), mvpmatx((m * v.view * v.proj).Transpose()) {}
};

struct alignas(256) objectconstants : public instance_data
{
    objectconstants() = default;
    objectconstants(matrix const& m, viewinfo const& v) : instance_data(m, v) {}
};

struct light
{
    vector3 color;
    float range;
    vector3 position;
    vector3 direction;
};

template<uint32 t_divisor>
constexpr inline uint32 divideup(uint32 value) requires (t_divisor > 0)
{
    return (value + t_divisor - 1) / t_divisor;
}

CD3DX12_RESOURCE_BARRIER reversetransition(CD3DX12_RESOURCE_BARRIER const& barrier)
{
    D3D12_RESOURCE_BARRIER d3d12barrier = barrier;
    auto statebefore = d3d12barrier.Transition.StateAfter;
    auto stateafter = d3d12barrier.Transition.StateBefore;

    return CD3DX12_RESOURCE_BARRIER::Transition(d3d12barrier.Transition.pResource, statebefore, stateafter);
}

// ray trace stuff below
struct trianglehitgroup
{
    std::string anyhit;
    std::string closesthit;
    std::string name;
};

struct proceduralhitgroup
{
    std::string anyhit;
    std::string intersection;
    std::string closesthit;
    std::string name;
};

struct raytraceshaders
{
    std::string raygen;
    std::string miss;
    trianglehitgroup tri_hitgrp;
    proceduralhitgroup procedural_hitgroup;
};

}