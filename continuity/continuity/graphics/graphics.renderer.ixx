module;

#include <d3d12.h>
#include <dxgi1_6.h>
#include "thirdparty/d3dx12.h"

export module graphics:renderer;

import stdxcore;
import :resourcetypes;
import :globalresources;

using Microsoft::WRL::ComPtr;

export namespace gfx
{

constexpr uint32 backbuffercount = 2;

class renderer
{
    HANDLE fenceevent{};
    UINT64 fencevalue = 0;
    UINT viewwidth;
    UINT viewheight;

    ComPtr<ID3D12Fence> fence;
    deviceresources d3ddevres;
    ComPtr<IDXGISwapChain3> swapchain;
    ComPtr<ID3D12Resource> backbuffers[backbuffercount];
    ComPtr<ID3D12CommandAllocator> commandallocators[1];
    ComPtr<ID3D12CommandQueue> commandqueue;
    resourcelist transientresources;
    
public:

    deviceresources& deviceres() { return d3ddevres; }
    resourcelist& transientres() { return transientresources; }

    gputexandidx finalcolour;
    gputexandidx environmenttex;
    texture<accesstype::gpu> depthtarget;
    texture<accesstype::gpu> shadowmap;

    rtheap rtheap;
    dtheap dtheap;
    resourceheap resheap;
    samplerheap sampheap;

    rtv rtview;
    dtv dtview;
    dtv shadowmapdtv;
    srv shadowmapsrv;

    uint32 frameidx = 0;

    void init(HWND window, UINT w, UINT h);
    void deinit();
    void createresources();
    void update(float dt);

    void prerender();
    void execute(renderpipeline const& pipeline);
    void postrender();

    void dispatchmesh(stdx::vecui3 dispatch, pipelinestate ps, std::vector<uint32> const& rootconsts);

    void waitforgpu();
};

}
