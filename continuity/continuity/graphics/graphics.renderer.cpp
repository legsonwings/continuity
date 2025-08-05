module;

#include <wrl.h>
#include <dxgi1_3.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "thirdparty/d3dx12.h"
#include "thirdparty/dxhelpers.h"

#include <shlobj.h>

module graphics:renderer;

import std;
import stdx;
import :globalresources;

using Microsoft::WRL::ComPtr;

static constexpr float clearcol[4] = { 0.254901975f, 0.254901975f, 0.254901975f, 1.f };

namespace gfx
{

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


// helper function for acquiring the first available hardware adapter that supports Direct3D 12
// if no such adapter can be found, *ppAdapter will be set to nullptr
_Use_decl_annotations_
void gethardwareadapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = false)
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
                // don't select the Basic Render Driver adapter
                // if you want a software adapter, pass in "/warp" on the command line
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
                // Don't select the Basic Render Driver adapter
                // If you want a software adapter, pass in "/warp" on the command line
                continue;
            }

            // check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
}

void renderer::init(HWND window, UINT w, UINT h)
{
    viewwidth = w;
    viewheight = h;

    viewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, float(w), float(h) };
    scissorrect = CD3DX12_RECT{ 0, 0, LONG(w), LONG(h) };

#if CONSOLE_LOGS
    AllocConsole();
    FILE* DummyPtr;
    freopen_s(&DummyPtr, "CONOUT$", "w", stdout);
#endif

    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // enable the debug layer (requires the Graphics Tools "optional feature")
    // note: enabling the debug layer after device creation will invalidate the active device
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

    auto& device = globalresources::get().device();
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    gethardwareadapter(factory.Get(), &hardwareAdapter);

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

    // describe and create the command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandqueue)));

    // describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapchaindesc = {};
    swapchaindesc.BufferCount = backbuffercount;
    swapchaindesc.Width = viewwidth;
    swapchaindesc.Height = viewheight;
    swapchaindesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // todo : use srgb format
    swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchaindesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> sc;

    // swap chain needs the queue so that it can force a flush on it
    ThrowIfFailed(factory->CreateSwapChainForHwnd(commandqueue.Get(), window, &swapchaindesc, nullptr, nullptr, &sc));

    // this sample does not support fullscreen transitions
    ThrowIfFailed(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(sc.As(&swapchain));

    // create descriptor heaps
    {
        // describe and create a render target view (RTV) descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvheapdesc = {};
        rtvheapdesc.NumDescriptors = backbuffercount;
        rtvheapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvheapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&rtvheapdesc, IID_PPV_ARGS(&rtvheap)));

        // describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC dsvheapdesc = {};
        dsvheapdesc.NumDescriptors = 2;
        dsvheapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvheapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device->CreateDescriptorHeap(&dsvheapdesc, IID_PPV_ARGS(&dsvheap)));
    }

    auto texdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<UINT64>(viewwidth), static_cast<UINT>(viewheight), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    D3D12_CLEAR_VALUE clearcolour = {};
    clearcolour.Format = texdesc.Format;
    clearcolour.Color[0] = clearcol[0];
    clearcolour.Color[1] = clearcol[1];
    clearcolour.Color[2] = clearcol[2];
    clearcolour.Color[3] = clearcol[3];

    auto defaultheap_desc = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(device->CreateCommittedResource(&defaultheap_desc, D3D12_HEAP_FLAG_NONE, &texdesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearcolour, IID_PPV_ARGS(rendertarget.ReleaseAndGetAddressOf())));

    NAME_D3D12_OBJECT(rendertarget);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvheap->GetCPUDescriptorHandleForHeapStart());
    device->CreateRenderTargetView(rendertarget.Get(), nullptr, rtvHandle);

    // create command allocator for only one frame since theres no frame buffering
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandallocators[0])));

    // get back buffers
    for (UINT n = 0; n < backbuffercount; n++)
        ThrowIfFailed(swapchain->GetBuffer(n, IID_PPV_ARGS(&backbuffers[n])));

    // create the depth stencil view
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 0.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto texture_resource_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, viewwidth, viewheight, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &texture_resource_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, IID_PPV_ARGS(&depthstencil)));

    NAME_D3D12_OBJECT(depthstencil);

    auto shadowmapdesc = texture_resource_desc;
    ThrowIfFailed(device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &shadowmapdesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, IID_PPV_ARGS(&shadowmap)));

    NAME_D3D12_OBJECT(shadowmap);

    auto dsvstart = dsvheap->GetCPUDescriptorHandleForHeapStart();
    auto shadowmap_cpuhandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvstart, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV));
    device->CreateDepthStencilView(depthstencil.Get(), &depthStencilDesc, dsvstart);
    device->CreateDepthStencilView(shadowmap.Get(), &depthStencilDesc, shadowmap_cpuhandle);

    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = rendertarget->GetDesc().Format;
    pso_desc.DSVFormat = depthstencil->GetDesc().Format;
    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);      // CW front; cull back
    pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);                // opaque
    pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // depth test w/ writes; no stencil
    pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.SampleDesc = DefaultSampleDesc();

    auto& globalres = globalresources::get();

    globalres.rthandle = rtvHandle;
    globalres.depthhandle = dsvstart;
    globalres.shadowmaphandle = shadowmap_cpuhandle;
    globalres.rendertarget(rendertarget);
    globalres.frameindex(swapchain->GetCurrentBackBufferIndex());
    globalres.psodesc(pso_desc);
    globalres.init();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
    srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvdesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvdesc.Texture2D.MipLevels = 1;

    globalres.shadowmapidx = globalres.resourceheap().addsrv(srvdesc, shadowmap.Get()).heapidx;
    globalres.shadowmap = shadowmap;

    auto& cmdlist = globalresources::get().cmdlist();

    // create the command list in recording state
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandallocators[0].Get(), nullptr, IID_PPV_ARGS(&cmdlist)));
}

void renderer::deinit()
{
    // ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor
    waitforgpu();

    CloseHandle(fenceevent);
}

void renderer::createresources()
{
    auto device = globalresources::get().device();
    auto cmdlist = globalresources::get().cmdlist();

    globalresources::get().create_resources();

    ThrowIfFailed(cmdlist->Close());

    ID3D12CommandList* ppCommandLists[] = { cmdlist.Get() };
    commandqueue->ExecuteCommandLists(1, ppCommandLists);

    // create synchronization objects
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    // why increment here?
    fencevalue++;

    // Create an event handle to use for frame synchronization
    fenceevent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceevent == nullptr)
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    // wait for the command list to execute; we are reusing the same command 
    // list in our main loop but for now, we just want to wait for setup to 
    // complete before continuing
    waitforgpu();
}

void renderer::update(float dt)
{
}

void renderer::prerender()
{
    auto cmdlist = globalresources::get().cmdlist();

    // command allocators can only be reset when the associated 
    // command lists have finished execution on the gpu
    ThrowIfFailed(commandallocators[0]->Reset());

    // however, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before re-recording
    ThrowIfFailed(cmdlist->Reset(commandallocators[0].Get(), nullptr));

    // set necessary state
    cmdlist->RSSetViewports(1, &viewport);
    cmdlist->RSSetScissorRects(1, &scissorrect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvheap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvheap->GetCPUDescriptorHandleForHeapStart());
    cmdlist->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    cmdlist->ClearRenderTargetView(rtvHandle, clearcol, 0, nullptr);
    cmdlist->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);
}

void renderer::postrender()
{
    auto cmdlist = globalresources::get().cmdlist();

    auto rttocopysource = CD3DX12_RESOURCE_BARRIER::Transition(rendertarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
    auto barrier_backbuffer = CD3DX12_RESOURCE_BARRIER::Transition(backbuffers[frameidx].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);

    cmdlist->ResourceBarrier(1, &rttocopysource);
    cmdlist->ResourceBarrier(1, &barrier_backbuffer);

    // copy from render target to back buffer
    cmdlist->CopyResource(backbuffers[frameidx].Get(), rendertarget.Get());

    auto barrier_backbuffer_restore = CD3DX12_RESOURCE_BARRIER::Transition(backbuffers[frameidx].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    auto copysrctort = CD3DX12_RESOURCE_BARRIER::Transition(rendertarget.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

    cmdlist->ResourceBarrier(1, &barrier_backbuffer_restore);
    cmdlist->ResourceBarrier(1, &copysrctort);

    ThrowIfFailed(cmdlist->Close());

    ID3D12CommandList* ppCommandLists[] = { cmdlist.Get() };
    commandqueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    ThrowIfFailed(swapchain->Present(1, 0));

    waitforgpu();

    frameidx = swapchain->GetCurrentBackBufferIndex();
}

// wait for pending gpu work to complete
void renderer::waitforgpu()
{
    // schedule a signal command in the queue
    ThrowIfFailed(commandqueue->Signal(fence.Get(), fencevalue));

    // wait until the fence has been processed
    ThrowIfFailed(fence->SetEventOnCompletion(fencevalue, fenceevent));
    WaitForSingleObjectEx(fenceevent, INFINITE, FALSE);

    // increment the fence value for the current frame
    fencevalue++;
}

}