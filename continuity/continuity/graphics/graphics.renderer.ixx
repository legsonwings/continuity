module;

#include <d3d12.h>
#include <dxgi1_6.h>
#include "thirdparty/d3dx12.h"

export module graphics:renderer;

import stdxcore;

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
    ComPtr<IDXGISwapChain3> swapchain;
    ComPtr<ID3D12Resource> backbuffers[backbuffercount];
    ComPtr<ID3D12Resource> rendertarget;
    ComPtr<ID3D12Resource> depthstencil;
    ComPtr<ID3D12Resource> shadowmap;
    ComPtr<ID3D12CommandAllocator> commandallocators[1];
    ComPtr<ID3D12CommandQueue> commandqueue;
    ComPtr<ID3D12DescriptorHeap> rtvheap;
    ComPtr<ID3D12DescriptorHeap> dsvheap;

public:
    void init(HWND window, UINT w, UINT h);
    void deinit();
    void createresources();
    void update(float dt);

    void prerender();
    void postrender();

    template<typename t>
    void render(t& sample, float dt)
    {
        prerender();
        sample.render(dt);
        postrender();
    }

    void waitforgpu();
};

}
