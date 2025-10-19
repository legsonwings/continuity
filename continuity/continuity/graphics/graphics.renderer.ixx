module;

#include <d3d12.h>
#include <dxgi1_6.h>
#include "thirdparty/d3dx12.h"

#include "shared/raytracecommon.h"

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
    
    texture<accesstype::gpu> environmenttex;

    stdx::ext<structuredbuffer<rt::accumparams, accesstype::both>, uint32> accumparamsbuffer;
    stdx::ext<structuredbuffer<rt::denoiserparams, accesstype::both>, uint32> denoiseparamsbuffer;
    stdx::ext<structuredbuffer<rt::postdenoiseparams, accesstype::both>, uint32> postdenoiseparamsbuffer;

    uint32 accumcount = 0;
public:

    deviceresources& deviceres() { return d3ddevres; }
    resourcelist& transientres() { return transientresources; }

    stdx::ext<texture<accesstype::gpu>, uint32> hitposition[2];
    stdx::ext<texture<accesstype::gpu>, uint32> normaldepthtex[2];
    stdx::ext<texture<accesstype::gpu>, uint32> diffusecolortex;
    stdx::ext<texture<accesstype::gpu>, uint32> specbrdftex;
    stdx::ext<texture<accesstype::gpu>, uint32> diffuseradiancetex[2];
    stdx::ext<texture<accesstype::gpu>, uint32> specularradiancetex[2];
    stdx::ext<texture<accesstype::gpu>, uint32> momentstex[2];

    // todo : encode normal into two floats and use remaining channel for depth instead of having two channel historylen
    stdx::ext<texture<accesstype::gpu>, uint32> historylentex[2];

    stdx::ext<texture<accesstype::gpu>, uint32> hdrrendertarget;
    stdx::ext<texture<accesstype::gpu>, uint32> rendertarget;
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

    // todo : hack 
    uint32 sceneglobals;
    uint32 viewglobals;
    uint32 ptsettings;

    uint32 envtexidx;
    uint32 frameidx = 0;
    bool spatialdenoise = true;

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
