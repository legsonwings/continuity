module;

// todo : anyway to avoid this?
#define __SPECSTRINGS_STRICT_LEVEL 0
#include <d3d12.h>
#include <dxgi1_6.h>
#include "thirdparty/d3dx12.h"

#include "simplemath/simplemath.h"

export module engine;
export import :steptimer;
export import :simplecamera;

import stdxcore;
import graphics;

export constexpr uint frame_count = 2;

using namespace DirectX;
using Microsoft::WRL::ComPtr;

export struct view_data
{
    unsigned width = 720;
    unsigned height = 720;
    float nearplane = 0.1f;
    float farplane = 1000.f;

    float get_aspect_ratio() const { return static_cast<float>(width) / static_cast<float>(height); }
};

// abstract base demo class
export class sample_base
{
public:
    sample_base(view_data const& data);

    virtual ~sample_base() {}

    virtual gfx::resourcelist create_resources() = 0;

    virtual void update(float dt) { updateview(dt); };
    virtual void render(float dt) = 0;

    virtual void on_key_down(unsigned key) { camera.OnKeyDown(key); };
    virtual void on_key_up(unsigned key) { camera.OnKeyUp(key); };

protected:
    simplecamera camera;

    void updateview(float dt);
};

// the "engine" class 
// handles win32 window and inputs and dx12 object setup(device, swapchains, rendertargets)
// todo : graphics module should handle device, swapchains, rendertargets, etc
export class continuity
{
public:
    continuity(view_data const& data = {});

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);

    static int Run(continuity* pSample, HINSTANCE hInstance, int nCmdShow);
    static HWND GetHwnd() { return m_hwnd; }

private:

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }

    void GetHardwareAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = false);
    void SetCustomWindowText(std::wstring const& text);

    // synchronization objects
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[frame_count];

    // pipeline objects
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Resource> m_renderTargets[frame_count];
    ComPtr<ID3D12Resource> m_depthStencil;
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[frame_count];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

    unsigned m_rtvDescriptorSize;
    unsigned m_dsvDescriptorSize;

    steptimer m_timer;
    std::unique_ptr<sample_base> sample;

    unsigned m_frameCounter;
    
    // viewport dimensions
    UINT m_width;
    UINT m_height;
    std::wstring m_title;

    static inline HWND m_hwnd = nullptr;

    void load_pipeline();
    void create_resources();
    void moveto_nextframe();
    void waitforgpu();   

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};