module;

#include <wrl.h>
#include <dxgi1_3.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "thirdparty/d3dx12.h"
#include "thirdparty/dxhelpers.h"
#include "DirectXMath.h"

#include <shlobj.h>
#include <strsafe.h>

#include "shared/sharedconstants.h"

module engine;

import activesample;
import graphics;

import std;

#define CONSOLE_LOGS 0

using namespace DirectX;

static constexpr float clearcol[4] = { 0.254901975f, 0.254901975f, 0.254901975f, 1.f };

sample_base::sample_base(view_data const& data) : viewdata(data)
{
    camera.nearplane(data.nearplane);
    camera.farplane(data.farplane);
    camera.width(data.width);
    camera.height(data.height);
}

void sample_base::updateview(float dt)
{
    camera.Update(dt);

    gfx::globalresources::get().view().view = camera.GetViewMatrix();
    gfx::globalresources::get().view().proj = camera.GetProjectionMatrix(XM_PI / 3.0f);
    gfx::globalresources::get().cbuffer().data().campos = camera.GetCurrentPosition();
    gfx::globalresources::get().cbuffer().updateresource();
}

continuity::continuity(view_data const& data)
    : m_width(data.width)
    , m_height(data.height)
    , m_viewport(0.0f, 0.0f, static_cast<float>(data.width), static_cast<float>(data.height))
    , m_scissorRect(0, 0, static_cast<LONG>(data.width), static_cast<LONG>(data.height))
    , m_frameCounter(0)
    , m_fenceEvent{}
    , m_fenceValues{}
{
    sample = sample_creator::create_instance<activesample>(data);
}

static std::wstring getlatest_winpixgpucapturer_path()
{
    LPWSTR programfilespath = nullptr;
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programfilespath);

    std::filesystem::path pixinstallationpath = programfilespath;
    pixinstallationpath /= "Microsoft PIX";

    std::wstring newestversionfound;

    for (auto const& directory_entry : std::filesystem::directory_iterator(pixinstallationpath))
        if (directory_entry.is_directory())
            if (newestversionfound.empty() || newestversionfound < directory_entry.path().filename().c_str())
                newestversionfound = directory_entry.path().filename().c_str();

    stdx::cassert(!newestversionfound.empty());
    return pixinstallationpath / newestversionfound / L"WinPixGpuCapturer.dll";
}

void continuity::OnInit()
{
#if CONSOLE_LOGS
    AllocConsole();
    FILE* DummyPtr;
    freopen_s(&DummyPtr, "CONOUT$", "w", stdout);
#endif

    load_pipeline();
    create_resources();
}

// load the rendering pipeline dependencies.
void continuity::load_pipeline()
{
    UINT dxgiFactoryFlags = 0;
 
#if defined(_DEBUG)
    // enable the debug layer (requires the Graphics Tools "optional feature").
    // note: enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug5> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            // enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

            debugController->EnableDebugLayer();
            debugController->SetEnableAutoName(true);
            debugController->SetEnableGPUBasedValidation(true);
        }
    }
#else

    static constexpr auto allowpixattach = false;

    // todo : this should be turned based on a based on a commmand line flag
    // WinPixGpuCapturer is not compatible with debug builds(debug layer conflicts?)
    
    if (allowpixattach && GetModuleHandleA("WinPixGpuCapturer.dll") == 0)
    {
        LoadLibrary(getlatest_winpixgpucapturer_path().c_str());
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    auto& device = gfx::globalresources::get().device();
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    GetHardwareAdapter(factory.Get(), &hardwareAdapter);

    ThrowIfFailed(D3D12CreateDevice(
        hardwareAdapter.Get(),
        D3D_FEATURE_LEVEL_12_1,
        IID_PPV_ARGS(device.ReleaseAndGetAddressOf())
    ));

    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_6 };
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))
        || (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_6))
    {
        OutputDebugStringA("ERROR: Shader Model 6.6 is not supported\n");
        throw std::exception("Shader Model 6.6 is not supported.");
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS features = {};
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &features, sizeof(features)))
        || (features.ResourceBindingTier != D3D12_RESOURCE_BINDING_TIER_3))
    {
        OutputDebugStringA("ERROR: Dynamic resources(Bindless resources) are not supported!\n");
        throw std::exception("Bindless resources aren't supported!");
    }
    
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5 = {};
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(features5))) || features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
        OutputDebugStringA("ERROR: Raytracing is not supported!\n");
        throw std::exception("Raytracing is not supported!");
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 features7 = {};
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &features7, sizeof(features7)))
        || (features7.MeshShaderTier == D3D12_MESH_SHADER_TIER_NOT_SUPPORTED))
    {
        OutputDebugStringA("ERROR: Mesh Shaders aren't supported!\n");
        throw std::exception("Mesh Shaders aren't supported!");
    }

    // describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = frame_count;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // todo : use srgb format
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        continuity::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(continuity::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
   
    // create descriptor heaps.
    {
        // describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = frame_count;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        // describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
    }

    // create render target
    {
        auto texdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT64>(m_width), static_cast<UINT>(m_height), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

        D3D12_CLEAR_VALUE clearcolour = {};
        clearcolour.Format = texdesc.Format;
        clearcolour.Color[0] = clearcol[0];
        clearcolour.Color[1] = clearcol[1];
        clearcolour.Color[2] = clearcol[2];
        clearcolour.Color[3] = clearcol[3];

        auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &texdesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearcolour, IID_PPV_ARGS(m_renderTarget.ReleaseAndGetAddressOf())));
        
        NAME_D3D12_OBJECT(m_renderTarget);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        device->CreateRenderTargetView(m_renderTarget.Get(), nullptr, rtvHandle);
    }

    // create command allocator for only one frame since theres no frame buffering
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[0])));

    // get back buffers
    for (UINT n = 0; n < frame_count; n++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_backBuffers[n])));
    }

    // create the depth stencil view.
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto texture_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        ThrowIfFailed(device->CreateCommittedResource(
            &heap_props,
            D3D12_HEAP_FLAG_NONE,
            &texture_resource_desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&m_depthStencil)
        ));

        NAME_D3D12_OBJECT(m_depthStencil);

        device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = m_renderTarget->GetDesc().Format;
    pso_desc.DSVFormat = m_depthStencil->GetDesc().Format;
    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);    // CW front; cull back
    pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);         // opaque
    pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // less-equal depth test w/ writes; no stencil
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.SampleDesc = DefaultSampleDesc();

    gfx::globalresources::get().rendertarget(m_renderTarget);
    gfx::globalresources::get().frameindex(m_swapChain->GetCurrentBackBufferIndex());
    gfx::globalresources::get().psodesc(pso_desc);
    gfx::globalresources::get().init();
}

// load the sample assets.
void continuity::create_resources()
{
    auto device = gfx::globalresources::get().device();
    auto& cmdlist = gfx::globalresources::get().cmdlist();

    // create the command list. They are created in recording state
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&cmdlist)));

    // need to keep these alive till data is uploaded to gpu
    std::vector<ComPtr<ID3D12Resource>> const gpu_resources = sample->create_resources();
    ThrowIfFailed(cmdlist->Close());

    ID3D12CommandList* ppCommandLists[] = { cmdlist.Get() };
    m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

    // create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValues[0]++;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        waitforgpu();
    }
}

void continuity::OnUpdate()
{
    m_timer.Tick(NULL);

    if (m_frameCounter++ % 30 == 0)
    {
        // update window text with FPS value.
        wchar_t fps[64];
        swprintf_s(fps, L"%ufps", m_timer.GetFramesPerSecond());

        std::wstring title = sample_titles[int(activesample)];
        if (title.empty())
            title = L"continuity";

        title = title + L": " + fps;
        SetCustomWindowText(title);
    }

    // update render target for current frame
    sample->update(static_cast<float>(m_timer.GetElapsedSeconds()));
}

// ender the scene.
void continuity::OnRender()
{
    auto const frameidx = gfx::globalresources::get().frameindex();
    auto cmdlist = gfx::globalresources::get().cmdlist();

    // command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocators[0]->Reset());

    // however, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(cmdlist->Reset(m_commandAllocators[0].Get(), nullptr));

    // set necessary state.
    cmdlist->RSSetViewports(1, &m_viewport);
    cmdlist->RSSetScissorRects(1, &m_scissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    cmdlist->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // record commands.
    cmdlist->ClearRenderTargetView(rtvHandle, clearcol, 0, nullptr);
    cmdlist->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    sample->render(static_cast<float>(m_timer.GetElapsedSeconds()));

    auto rttocopysource = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    auto barrier_backbuffer = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[frameidx].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);

    cmdlist->ResourceBarrier(1, &rttocopysource);
    cmdlist->ResourceBarrier(1, &barrier_backbuffer);

    // copy from render target to back buffer
    cmdlist->CopyResource(m_backBuffers[frameidx].Get(), m_renderTarget.Get());

    auto barrier_backbuffer_restore = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[frameidx].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    auto copysrctort = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    cmdlist->ResourceBarrier(1, &barrier_backbuffer_restore);
    cmdlist->ResourceBarrier(1, &copysrctort);

    ThrowIfFailed(cmdlist->Close());

    // execute the command list.
    ID3D12CommandList* ppCommandLists[] = { cmdlist.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    waitforgpu();

    gfx::globalresources::get().frameindex(m_swapChain->GetCurrentBackBufferIndex());
}

void continuity::OnDestroy()
{
    // ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    waitforgpu();

    CloseHandle(m_fenceEvent);
}

void continuity::OnKeyDown(UINT8 key)
{
    sample->on_key_down(key);
}

void continuity::OnKeyUp(UINT8 key)
{
    sample->on_key_up(key);
}

// wait for pending GPU work to complete.
void continuity::waitforgpu()
{
    // schedule a Signal command in the queue.
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[0]));

    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[0], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

    // increment the fence value for the current frame.
    m_fenceValues[0]++;
}

// helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// if no such adapter can be found, *ppAdapter will be set to nullptr.
_Use_decl_annotations_
void continuity::GetHardwareAdapter(
    IDXGIFactory1* pFactory,
    IDXGIAdapter1** ppAdapter,
    bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (UINT adapterIndex = 0;; ++adapterIndex)
        {
            auto ret_code = factory6->EnumAdapterByGpuPreference(adapterIndex, requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED, IID_PPV_ARGS(&adapter));
            if (ret_code == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }

            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // don't select the Basic Render Driver adapter.
                // if you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }
    else
    {
        for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
}

// helper function for setting the window's title text.
void continuity::SetCustomWindowText(std::wstring const& text)
{
    std::wstring windowText = m_title + L": " + text;
    SetWindowText(continuity::GetHwnd(), windowText.c_str());
}

int continuity::Run(continuity* pSample, HINSTANCE hInstance, int nCmdShow)
{
    // parse the command line parameters
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    //pSample->ParseCommandLineArgs(argv, argc);
    LocalFree(argv);

    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"DXSampleClass";
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, static_cast<LONG>(pSample->GetWidth()), static_cast<LONG>(pSample->GetHeight()) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        pSample->GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // we have no parent window.
        nullptr,        // we aren't using menus.
        hInstance,
        pSample);

    // initialize the sample. OnInit is defined in each child-implementation of DXSample.
    pSample->OnInit();

    ShowWindow(m_hwnd, nCmdShow);

    // Main sample loop.
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    pSample->OnDestroy();

    // return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}

// main message handler for the sample.
LRESULT CALLBACK continuity::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    continuity* pSample = reinterpret_cast<continuity*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        // save the sample object pointer passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
    return 0;

    case WM_KEYDOWN:
        if (pSample)
        {
            pSample->OnKeyDown(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_KEYUP:
        if (pSample)
        {
            pSample->OnKeyUp(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_PAINT:
        if (pSample)
        {
            pSample->OnUpdate();
            pSample->OnRender();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    // handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}
