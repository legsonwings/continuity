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
    struct accumparams
    {
        uint32 currtexture;
        uint32 hdrcolourtex;
        uint32 accumcount;
    };

    HANDLE fenceevent{};
    UINT64 fencevalue = 0;
    UINT viewwidth;
    UINT viewheight;
    uint32 frameidx = 0;

    uav hdrrtuav;
    uav rtuav;

    ComPtr<ID3D12Fence> fence;
    deviceresources d3ddevres;
    ComPtr<IDXGISwapChain3> swapchain;
    ComPtr<ID3D12Resource> backbuffers[backbuffercount];
    ComPtr<ID3D12CommandAllocator> commandallocators[1];
    ComPtr<ID3D12CommandQueue> commandqueue;
    resourcelist transientresources;
    
    texture<accesstype::gpu> environmenttex;
    
    uint32 accumparamsidx;

    gfx::structuredbuffer<accumparams, gfx::accesstype::both> accumparamsbuffer;

    uint32 accumcount = 0;
public:

    deviceresources& deviceres() { return d3ddevres; }
    resourcelist& transientres() { return transientresources; }

    texture<accesstype::gpu> hdrrendertarget;
    texture<accesstype::gpu> rendertarget;
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

    uint32 envtexidx;

    void init(HWND window, UINT w, UINT h);
    void deinit();
    void createresources();
    void update(float dt);

    void prerender();
    void postrender();

    void dispatchmesh(stdx::vecui3 dispatch, pipelinestate ps, std::vector<uint32> const& rootconsts);

    void waitforgpu();
    void clearaccumcount();
};

}
