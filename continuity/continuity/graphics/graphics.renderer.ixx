module;

#include <d3d12.h>
#include <dxgi1_6.h>
#include "thirdparty/d3dx12.h"

export module graphics:renderer;

import stdxcore;
import :resourcetypes;

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
    uint32 frameidx = 0;

    CD3DX12_VIEWPORT viewport;
    CD3DX12_RECT scissorrect;

    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12Device5> device;
    ComPtr<IDXGISwapChain3> swapchain;
    ComPtr<ID3D12GraphicsCommandList6> cmdlist;
    ComPtr<ID3D12Resource> backbuffers[backbuffercount];
    ComPtr<ID3D12CommandAllocator> commandallocators[1];
    ComPtr<ID3D12CommandQueue> commandqueue;

    texture<accesstype::gpu> rendertarget;
    texture<accesstype::gpu> depthtarget;
    texture<accesstype::gpu> shadowmap;
    rtheap rtheap;
    dtheap dtheap;
    rtv rtview;
    dtv dtview;
    dtv shadowmapview;

public:

    void init(HWND window, UINT w, UINT h);
    void deinit();
    void createresources();
    void update(float dt);

    void prerender();
    void postrender();

    void waitforgpu();
};

}
