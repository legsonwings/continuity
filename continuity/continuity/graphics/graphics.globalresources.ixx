module;

#include "thirdparty/d3dx12.h"

#include <wrl.h>
#include <d3d12.h>

export module graphics:globalresources;

import stdxcore;
import stdx;
import std;
import :resourcetypes;

import graphicscore;

using Microsoft::WRL::ComPtr;

export namespace gfx
{

using psomapref = std::unordered_map<std::string, pipeline_objects> const&;
using matmapref = std::unordered_map<std::string, stdx::ext<material, bool>> const&;
using materialentry = stdx::ext<material, bool>;
using materialcref = stdx::ext<material, bool> const&;

class globalresources
{
	viewinfo _view;
	uint _frameindex{ 0 };
	std::string _assetspath;
	resourceheap _resourceheap;
	samplerheap _samplerheap;
	ComPtr<ID3D12Device5> _device;

	ComPtr<ID3D12Resource> _rendertarget;
	ComPtr<ID3D12GraphicsCommandList6> _commandlist;
	std::unordered_map<std::string, pipeline_objects> _psos;
	stdx::ext<material, bool> _defaultmat{ {}, false };

	std::vector<material> _materials;
	std::unordered_map<DXGI_FORMAT, uint> _dxgisizes{ {DXGI_FORMAT_R8G8B8A8_UNORM, 4} };
	D3DX12_MESH_SHADER_PIPELINE_STATE_DESC _psodesc{};

	uint32 _materialsbuffer_idx = stdx::invalid<uint32>;;
	gfx::structuredbuffer<material, gfx::accesstype::both> materialsbuffer;

public:

	uint32 shadowmapidx;
	ComPtr<ID3D12Resource> shadowmap;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rthandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE depthhandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE shadowmaphandle;

	void init();
	void deinit();
	void create_resources();

	static constexpr uint32 max_resdescriptors = 10000;
	static constexpr uint32 max_samplerdescriptors = 2048;
	static constexpr uint32 max_materials = 1000;

	viewinfo& view();
	psomapref psomap() const;
	matmapref matmap() const;
	uint32 materialsbuffer_idx() const;
	materialcref defaultmat() const;
	void rendertarget(ComPtr<ID3D12Resource>& rendertarget);
	ComPtr<ID3D12Resource>& rendertarget();
	ComPtr<ID3D12Device5>& device();
	resourceheap& resourceheap();
	samplerheap& samplerheap();
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
	std::string assetfullpath(std::string const& path) const;

	static globalresources& get();
};

}
