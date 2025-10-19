module;

#include <wrl.h>
#include <dxgi1_3.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "thirdparty/d3dx12.h"
#include "thirdparty/dxhelpers.h"
#include <shlobj.h>

#include "shared/raytracecommon.h"

module graphics:renderer;

import std;
import stdx;
import graphicscore;
import :globalresources;
import engine;

using Microsoft::WRL::ComPtr;
using namespace stdx;

static constexpr stdx::vec4 clearcol = { 0, 0, 0, 0 };
static constexpr stdx::vec4 cleardepth = { 0.0f, 0.0f, 0.0f, 0.0f };

// needs a callable createtexanduav
#define createtexanduav_wrap(tex, desc) createtexanduav(#tex, tex, desc)

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

    ComPtr<IDXGIAdapter1> hardwareAdapter;
    gethardwareadapter(factory.Get(), &hardwareAdapter);

    ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(d3ddevres.dev.ReleaseAndGetAddressOf())));

    // todo : temp
    globalresources::get().device() = d3ddevres.dev;

    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_6 };
    if (FAILED(d3ddevres.dev->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)))
        || (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_6))
    {
        OutputDebugStringA("ERROR: Shader Model 6.6 is not supported\n");
        throw std::exception("Shader Model 6.6 is not supported.");
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS features = {};
    if (FAILED(d3ddevres.dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &features, sizeof(features)))
        || (features.ResourceBindingTier != D3D12_RESOURCE_BINDING_TIER_3))
    {
        OutputDebugStringA("ERROR: Dynamic resources(Bindless resources) are not supported!\n");
        throw std::exception("Bindless resources aren't supported!");
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5 = {};
    if (FAILED(d3ddevres.dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(features5))) || features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
        OutputDebugStringA("ERROR: Raytracing is not supported!\n");
        throw std::exception("Raytracing is not supported!");
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 features7 = {};
    if (FAILED(d3ddevres.dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &features7, sizeof(features7)))
        || (features7.MeshShaderTier == D3D12_MESH_SHADER_TIER_NOT_SUPPORTED))
    {
        OutputDebugStringA("ERROR: Mesh Shaders aren't supported!\n");
        throw std::exception("Mesh Shaders aren't supported!");
    }

    // describe and create the command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(d3ddevres.dev->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandqueue)));

    // describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapchaindesc = {};
    swapchaindesc.BufferCount = backbuffercount;
    swapchaindesc.Width = viewwidth;
    swapchaindesc.Height = viewheight;
    swapchaindesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchaindesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> sc;

    // swap chain needs the queue so that it can force a flush on it
    ThrowIfFailed(factory->CreateSwapChainForHwnd(commandqueue.Get(), window, &swapchaindesc, nullptr, nullptr, &sc));

    // this sample does not support fullscreen transitions
    ThrowIfFailed(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(sc.As(&swapchain));

    rtheap.d3dheap = createdescriptorheap(rtheap::maxdescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dtheap.d3dheap = createdescriptorheap(dtheap::maxdescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    resheap.d3dheap = createdescriptorheap(resourceheap::maxdescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    sampheap.d3dheap = createdescriptorheap(samplerheap::maxdescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    auto& globalres = globalresources::get();

    // todo : temp
    globalres.resourceheap() = resheap;
    globalres.samplerheap() = sampheap;

    auto createtexanduav = [&](char const* name, auto& tex, auto const& desc)
    {
        tex->create(desc, clearcol, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        tex->d3dresource->SetName(utils::strtowstr(name).c_str());
        tex.ex() = tex->createuav().heapidx;
    };

    auto rtdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, UINT64(viewwidth), UINT(viewheight), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    auto hdrrtdesc = rtdesc;
    hdrrtdesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

    createtexanduav_wrap(rendertarget, rtdesc);
    createtexanduav_wrap(hdrrendertarget, hdrrtdesc);
    createtexanduav_wrap(normaldepthtex[0], hdrrtdesc);
    createtexanduav_wrap(normaldepthtex[1], hdrrtdesc);
    createtexanduav_wrap(hitposition[0], hdrrtdesc);
    createtexanduav_wrap(hitposition[1], hdrrtdesc);
    createtexanduav_wrap(diffusecolortex, hdrrtdesc);
    createtexanduav_wrap(specbrdftex, hdrrtdesc);
    createtexanduav_wrap(diffuseradiancetex[0], hdrrtdesc);
    createtexanduav_wrap(diffuseradiancetex[1], hdrrtdesc);
    createtexanduav_wrap(specularradiancetex[0], hdrrtdesc);
    createtexanduav_wrap(specularradiancetex[1], hdrrtdesc);
    createtexanduav_wrap(historylentex[0], hdrrtdesc);
    createtexanduav_wrap(historylentex[1], hdrrtdesc);

    // todo : can moments be 16 bits
    hdrrtdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    // todo : don't need second moments texture
    createtexanduav_wrap(momentstex[0], hdrrtdesc);
    createtexanduav_wrap(momentstex[1], hdrrtdesc);

    auto dtdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, viewwidth, viewheight, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    depthtarget.create(dtdesc, cleardepth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    shadowmap.create(dtdesc, cleardepth, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    rtview = rendertarget->creatertv(rtheap);
    dtview = depthtarget.createdtv(dtheap);
    shadowmapdtv = shadowmap.createdtv(dtheap);

    // create command allocator for only one frame since theres no frame buffering
    ThrowIfFailed(d3ddevres.dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandallocators[0])));

    // get back buffers
    for (UINT n = 0; n < backbuffercount; n++)
        ThrowIfFailed(swapchain->GetBuffer(n, IID_PPV_ARGS(&backbuffers[n])));

    // create the depth stencil view
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = rendertarget->d3dresource->GetDesc().Format;
    pso_desc.DSVFormat = depthtarget.d3dresource->GetDesc().Format;
    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);      // CW front; cull back
    pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);                // opaque
    pso_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // depth test w/ writes; no stencil
    pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.SampleDesc = DefaultSampleDesc();

    // todo : these shouldn't be passed using global resources but use renderer functions to access
    globalres.psodesc(pso_desc);
    globalres.init();

    // create the command list in recording state
    ThrowIfFailed(d3ddevres.dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandallocators[0].Get(), nullptr, IID_PPV_ARGS(&d3ddevres.cmdlist)));

    globalres.cmdlist() = d3ddevres.cmdlist;

    shadowmapsrv = shadowmap.createsrv(DXGI_FORMAT_R32_FLOAT);

    stdx::vecui2 diffradianceidx = { diffuseradiancetex[0].ex(), diffuseradiancetex[1].ex() };
    stdx::vecui2 specradianceidx = { specularradiancetex[0].ex(), specularradiancetex[1].ex() };
    stdx::vecui2 normaldepthidx = { normaldepthtex[0].ex(), normaldepthtex[1].ex() };
    stdx::vecui2 histlenddxyidx = { historylentex[0].ex(), historylentex[1].ex() };
    stdx::vecui2 hitpositionidx = { hitposition[0].ex(), hitposition[1].ex() };

    accumparamsbuffer->create();
    accumparamsbuffer.ex() = accumparamsbuffer->createsrv().heapidx;
    d3ddevres.hdrrtuavidx = hdrrendertarget.ex();
    d3ddevres.normaldepthuavidx = normaldepthidx;
    d3ddevres.historylenuavidx = histlenddxyidx;
    d3ddevres.diffcoloruavidx = diffusecolortex.ex();
    d3ddevres.specbrdfuavidx = specbrdftex.ex();
    d3ddevres.diffradianceuavidx = diffradianceidx;
    d3ddevres.specradianceuavidx = specradianceidx;
    d3ddevres.hitposition = hitpositionidx;
}

void renderer::deinit()
{
    // ensure that the GPU is no longer referencing resources that are about to be cleaned up by the destructor
    waitforgpu();

    CloseHandle(fenceevent);
}

void renderer::createresources()
{
    globalresources::get().create_resources();

    auto const& uploadtex = environmenttex.createfromfile("textures/sky.hdr");
    envtexidx = environmenttex.createsrv().heapidx;

    ThrowIfFailed(d3ddevres.cmdlist->Close());

    ID3D12CommandList* ppCommandLists[] = { d3ddevres.cmdlist.Get() };
    commandqueue->ExecuteCommandLists(1, ppCommandLists);

    // create synchronization objects
    ThrowIfFailed(d3ddevres.dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

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
    // command allocators can only be reset when the associated 
    // command lists have finished execution on the gpu
    ThrowIfFailed(commandallocators[0]->Reset());

    // however, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before re-recording
    ThrowIfFailed(d3ddevres.cmdlist->Reset(commandallocators[0].Get(), nullptr));
}

void renderer::postrender()
{
    auto& globalres = globalresources::get();

    stdx::vecui2 diffradianceidx = { diffuseradiancetex[0].ex(), diffuseradiancetex[1].ex() };
    stdx::vecui2 specradianceidx = { specularradiancetex[0].ex(), specularradiancetex[1].ex() };
    stdx::vecui2 normaldepthidx = { normaldepthtex[0].ex(), normaldepthtex[1].ex() };
    stdx::vecui2 histlenddxyidx = { historylentex[0].ex(), historylentex[1].ex() };
    stdx::vecui2 hitpositionidx = { hitposition[0].ex(), hitposition[1].ex() };

    if (!denoiseparamsbuffer->d3dresource)
    {
        rt::denoiserparams params
        {
            diffradianceidx, specradianceidx, normaldepthidx, histlenddxyidx, hitpositionidx, { uint32(viewwidth), uint32(viewheight) }, sceneglobals
        };

        denoiseparamsbuffer->create({ params });
        denoiseparamsbuffer.ex() = denoiseparamsbuffer->createsrv().heapidx;

        stdx::cassert(!postdenoiseparamsbuffer->d3dresource);

        rt::postdenoiseparams postdenparams = { diffusecolortex.ex(), specbrdftex.ex(), diffradianceidx, specradianceidx, rendertarget.ex(), sceneglobals};
        postdenoiseparamsbuffer->create({ postdenparams });
        postdenoiseparamsbuffer.ex() = postdenoiseparamsbuffer->createsrv().heapidx;
    }

    ID3D12DescriptorHeap* heaps[] = { resheap.d3dheap.Get(), sampheap.d3dheap.Get() };

    auto const& accumpipeobjs = globalres.psomap().find("temporalaccum")->second;
    
    // all compute pso's have the same root signature so just set it once
    d3ddevres.cmdlist->SetComputeRootSignature(accumpipeobjs.root_signature.Get());
   
    auto dispatchx = UINT(divideup<8>(viewwidth));
    auto dispatchy = UINT(divideup<8>(viewheight));

    CD3DX12_RESOURCE_BARRIER preaccumbarriers[] =
    {
        CD3DX12_RESOURCE_BARRIER::UAV(hdrrendertarget->d3dresource.Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(normaldepthtex[0]->d3dresource.Get()),
        CD3DX12_RESOURCE_BARRIER::UAV(normaldepthtex[1]->d3dresource.Get()),
    };

    d3ddevres.cmdlist->ResourceBarrier(_countof(preaccumbarriers), preaccumbarriers);

    // todo: revisit alpha after spatial denoising
    float alpha = 0.1f;
    rt::accumparams params
    { 
        rendertarget.ex(), hdrrendertarget.ex(), accumcount++, ptsettings, viewglobals, sceneglobals, { hitposition[0].ex(), hitposition[1].ex() }, { diffuseradiancetex[0].ex(), diffuseradiancetex[1].ex() }, {specularradiancetex[0].ex(), specularradiancetex[1].ex()},
        { normaldepthtex[0].ex(), normaldepthtex[1].ex() }, { historylentex[0].ex(), historylentex[1].ex()}, { momentstex[0].ex(), momentstex[1].ex()},
        {uint32(viewwidth), uint32(viewheight)}, alpha, alpha
    };

    accumparamsbuffer->update({ params });

    d3ddevres.cmdlist->SetPipelineState(accumpipeobjs.pso.Get());
    d3ddevres.cmdlist->SetComputeRoot32BitConstants(0, 3, &accumparamsbuffer.ex(), 0);
    d3ddevres.cmdlist->Dispatch(dispatchx, dispatchy, 1);

    if (spatialdenoise)
    {
        // we only need to barrier resources written this frame, however due to pingpong nature add all of them
        CD3DX12_RESOURCE_BARRIER predenoisebarriers[] =
        {
            CD3DX12_RESOURCE_BARRIER::UAV(historylentex[0]->d3dresource.Get()),
            CD3DX12_RESOURCE_BARRIER::UAV(historylentex[1]->d3dresource.Get()),
            CD3DX12_RESOURCE_BARRIER::UAV(momentstex[0]->d3dresource.Get()),
            CD3DX12_RESOURCE_BARRIER::UAV(momentstex[1]->d3dresource.Get()),
            CD3DX12_RESOURCE_BARRIER::UAV(diffusecolortex->d3dresource.Get()),
            CD3DX12_RESOURCE_BARRIER::UAV(diffuseradiancetex[0]->d3dresource.Get()),
            CD3DX12_RESOURCE_BARRIER::UAV(specularradiancetex[0]->d3dresource.Get()),
            CD3DX12_RESOURCE_BARRIER::UAV(diffuseradiancetex[1]->d3dresource.Get()),
            CD3DX12_RESOURCE_BARRIER::UAV(specularradiancetex[1]->d3dresource.Get())
        };

        d3ddevres.cmdlist->ResourceBarrier(_countof(predenoisebarriers), predenoisebarriers);

        auto const& denoisepipeobjs = globalres.psomap().find("denoise")->second;

        d3ddevres.cmdlist->SetPipelineState(denoisepipeobjs.pso.Get());

        rt::denoiserootconsts denrootconsts = { denoiseparamsbuffer.ex() };
        for (uint32 i = 0; i < 1; ++i)
        {
            denrootconsts.passidx = i;

            d3ddevres.cmdlist->SetComputeRoot32BitConstants(0, 2, &denrootconsts, 0);
            d3ddevres.cmdlist->Dispatch(dispatchx, dispatchy, 1);

            CD3DX12_RESOURCE_BARRIER barriersradiance[] =
            {
                CD3DX12_RESOURCE_BARRIER::UAV(diffuseradiancetex[0]->d3dresource.Get()), CD3DX12_RESOURCE_BARRIER::UAV(diffuseradiancetex[1]->d3dresource.Get()),
                CD3DX12_RESOURCE_BARRIER::UAV(specularradiancetex[0]->d3dresource.Get()), CD3DX12_RESOURCE_BARRIER::UAV(specularradiancetex[1]->d3dresource.Get())
            };

            d3ddevres.cmdlist->ResourceBarrier(_countof(barriersradiance), barriersradiance);
        }

        auto const& postdenoisepipeobjs = globalres.psomap().find("postdenoise")->second;

        // todo : filter moments and illumination bilaterally before denosing(for low accumulated samples)
        d3ddevres.cmdlist->SetPipelineState(postdenoisepipeobjs.pso.Get());
        d3ddevres.cmdlist->SetComputeRoot32BitConstants(0, 1, &postdenoiseparamsbuffer.ex(), 0);
        d3ddevres.cmdlist->Dispatch(dispatchx, dispatchy, 1);
    }

    auto rtbarrier = CD3DX12_RESOURCE_BARRIER::UAV(rendertarget->d3dresource.Get());
    d3ddevres.cmdlist->ResourceBarrier(1, &rtbarrier);

    // no uav barrier needed for rendertarget due to transition
    CD3DX12_RESOURCE_BARRIER transitions[2];
    transitions[0] = CD3DX12_RESOURCE_BARRIER::Transition(rendertarget->d3dresource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    transitions[1] = CD3DX12_RESOURCE_BARRIER::Transition(backbuffers[frameidx].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);

    d3ddevres.cmdlist->ResourceBarrier(_countof(transitions), transitions);

    // copy from render target to back buffer
    d3ddevres.cmdlist->CopyResource(backbuffers[frameidx].Get(), rendertarget->d3dresource.Get());

    for (auto& t : transitions)
        t = reversetransition(t);

    d3ddevres.cmdlist->ResourceBarrier(_countof(transitions), transitions);

    ThrowIfFailed(d3ddevres.cmdlist->Close());

    ID3D12CommandList* ppCommandLists[] = { d3ddevres.cmdlist.Get() };
    commandqueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    ThrowIfFailed(swapchain->Present(1, 0));

    waitforgpu();

    resheap.cleartransients();
    transientresources.clear();

    frameidx = swapchain->GetCurrentBackBufferIndex();
}

void renderer::dispatchmesh(stdx::vecui3 dispatch, pipelinestate ps, std::vector<uint32> const& rootdescs)
{
    auto& globalres = globalresources::get();
    auto const psobjs = globalres.psomap().find(ps.psoname)->second;

    static constexpr uint32 dispatch1dlimit = 65536;

    for (uint i(0); i < 3; ++i) stdx::cassert(dispatch[i] < dispatch1dlimit);

    UINT numrts = ps.rthandle.ptr != 0 ? 1 : 0;
    D3D12_CPU_DESCRIPTOR_HANDLE const* rts = numrts == 0 ? nullptr : &ps.rthandle;
    D3D12_CPU_DESCRIPTOR_HANDLE const* dt = ps.dthandle.ptr == 0 ? nullptr : &ps.dthandle;

    d3ddevres.cmdlist->OMSetRenderTargets(numrts, rts, FALSE, dt);

    CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, float(ps.viewportsize[0]), float(ps.viewportsize[1])};
    CD3DX12_RECT scissorrect = CD3DX12_RECT{ 0, 0, LONG(ps.viewportsize[0]), LONG(ps.viewportsize[1]) };

    d3ddevres.cmdlist->RSSetViewports(1, &viewport);
    d3ddevres.cmdlist->RSSetScissorRects(1, &scissorrect);

    static const FLOAT clearcola[4] = { clearcol[0], clearcol[1], clearcol[2], clearcol[3] };

    if(rts != nullptr)
        d3ddevres.cmdlist->ClearRenderTargetView(ps.rthandle, clearcola, 0, nullptr);

    if(dt != nullptr)
        d3ddevres.cmdlist->ClearDepthStencilView(ps.dthandle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);

    d3ddevres.cmdlist->SetPipelineState(psobjs.pso.Get());
    d3ddevres.cmdlist->SetGraphicsRootSignature(psobjs.root_signature.Get());

    ID3D12DescriptorHeap* heaps[] = { resheap.d3dheap.Get(), sampheap.d3dheap.Get() };
    d3ddevres.cmdlist->SetDescriptorHeaps(_countof(heaps), heaps);

    d3ddevres.cmdlist->SetGraphicsRoot32BitConstants(0, UINT(rootdescs.size()), rootdescs.data(), 0);

    d3ddevres.cmdlist->DispatchMesh(UINT(dispatch[0]), UINT(dispatch[1]), UINT(dispatch[2]));
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

void renderer::clearaccumcount()
{
    accumcount = 0;
}

}