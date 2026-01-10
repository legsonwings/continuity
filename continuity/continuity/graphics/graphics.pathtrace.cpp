module;

#include "thirdparty/d3dx12.h"
#include "shared/raytracecommon.h"

module graphics:pathtrace;

import :globalresources;
import :renderpasses;

// needs a callable createtexanduav
#define createtexanduav_wrap(tex, desc) createtexanduav(#tex, tex, desc)

namespace gfx
{

void ptresources::create(rt::ptsettings const& ptsettings, stdx::vecui2 dims, uint32 colour, uint32 viewglobalsidx, uint32 sceneglobalsidx)
{
    finalcolour = colour;
    viewglobals = viewglobalsidx;
    sceneglobals = sceneglobalsidx;

    auto hdrrtdesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, UINT64(dims[0]), UINT(dims[1]), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

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

    ptsettingsbuffer->create({ ptsettings });
    ptsettingsbuffer.ex() = ptsettingsbuffer->createsrv().heapidx;
}

pathtracepass::pathtracepass(ptbuffers const& ptbuff, ptresources& ptres, uint32 ptspp, resource const& raygenshadertable) : res(ptres), spp(ptspp), raygenst(raygenshadertable)
{
    auto dims = res.diffusecolortex->dims;
    rt::dispatchparams dispatch_params;
    dispatch_params.posbuffer = ptbuff.pos;
    dispatch_params.texcoordbuffer = ptbuff.texcoord;
    dispatch_params.tbnbuffer = ptbuff.tbn;
    dispatch_params.primitivematerialsbuffer = ptbuff.materials;
    dispatch_params.viewglobals = ptres.viewglobals;
    dispatch_params.sceneglobals = ptres.sceneglobals;
    dispatch_params.accelerationstruct = ptbuff.tlas;
    dispatch_params.dims = dims;
    dispatch_params.indexbuffer = ptbuff.idx;
    dispatch_params.ptsettings = res.ptsettingsbuffer.ex();
    dispatch_params.normaldepthtex = { res.normaldepthtex[0].ex(), res.normaldepthtex[1].ex() };
    dispatch_params.historylentex = { res.historylentex[0].ex(), res.historylentex[1].ex() };
    dispatch_params.diffcolortex = res.diffusecolortex.ex();
    dispatch_params.specbrdftex = res.specbrdftex.ex();
    dispatch_params.diffradiancetex = { res.diffuseradiancetex[0].ex(), res.diffuseradiancetex[1].ex() };
    dispatch_params.specradiancetex = { res.specularradiancetex[0].ex(), res.specularradiancetex[1].ex() };
    dispatch_params.rtoutput = res.hdrrendertarget.ex();
    dispatch_params.hitposition = { res.hitposition[0].ex(), res.hitposition[1].ex() };

    dispatchparams.create({ dispatch_params });

    rootdescs.rootdesc = dispatchparams.createsrv().heapidx;
}

void pathtracepass::operator()(deviceresources& devres)
{
    auto dims = res.diffusecolortex->dims;
    auto& globalres = globalresources::get();

    auto& cmdlist = devres.cmdlist;
    auto const& pipelineobjects = globalres.psomap().find("pathtrace")->second;

    cmdlist->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    cmdlist->SetPipelineState1(pipelineobjects.pso_raytracing.Get());

    for (auto i : stdx::range(spp))
    {
        rootdescs.sampleidx = i;
        cmdlist->SetComputeRoot32BitConstants(0, 2, &rootdescs, 0);

        // this function expects resources and state to be setup
        D3D12_DISPATCH_RAYS_DESC dispatchdesc = {};
        dispatchdesc.RayGenerationShaderRecord.StartAddress = raygenst.gpuaddress();
        dispatchdesc.RayGenerationShaderRecord.SizeInBytes = raygenst.d3dresource->GetDesc().Width;
        dispatchdesc.Width = UINT(dims[0]);
        dispatchdesc.Height = UINT(dims[1]);
        dispatchdesc.Depth = 1;

        cmdlist->DispatchRays(&dispatchdesc);

        uavbarrierpass
        {
            res.hdrrendertarget, res.diffusecolortex, res.diffuseradiancetex[0], res.diffuseradiancetex[1], res.specularradiancetex[0], res.specularradiancetex[1],
            res.historylentex[0], res.historylentex[1], res.hitposition[0], res.hitposition[1], res.normaldepthtex[0], res.normaldepthtex[1]
        }(devres);
    }
}

temporalaccumpass::temporalaccumpass(ptresources& ptres) : res(ptres), accumcount(0)
{
    auto dims = res.diffusecolortex->dims;
    rt::accumparams accparams
    {
        res.finalcolour, res.hdrrendertarget.ex(), 0, res.ptsettingsbuffer.ex(), res.viewglobals, res.sceneglobals, { res.hitposition[0].ex(), res.hitposition[1].ex() }, { res.diffuseradiancetex[0].ex(), res.diffuseradiancetex[1].ex() },
        { res.specularradiancetex[0].ex(), res.specularradiancetex[1].ex() }, { res.normaldepthtex[0].ex(), res.normaldepthtex[1].ex() }, { res.historylentex[0].ex(), res.historylentex[1].ex()}, { res.momentstex[0].ex(), res.momentstex[1].ex() }, dims
    };

    dispatchxy = { divideup<8>(dims[0]), divideup<8>(dims[1]) };
    accumparamsbuffer->create({ accparams });
    accumparamsbuffer.ex() = accumparamsbuffer->createsrv().heapidx;
}

void temporalaccumpass::operator()(deviceresources& devres)
{
    rt::accumparams& mappedparams = accumparamsbuffer[0];
    mappedparams.accumcount = accumcount++;
    auto& globalres = globalresources::get();
    auto const& pipelineobjects = globalres.psomap().find("temporalaccum")->second;

    devres.cmdlist->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    devres.cmdlist->SetPipelineState(pipelineobjects.pso.Get());

    devres.cmdlist->SetComputeRoot32BitConstants(0, 3, &accumparamsbuffer.ex(), 0);
    devres.cmdlist->Dispatch(UINT(dispatchxy[0]), UINT(dispatchxy[1]), 1);
}

atrousdenoisepass::atrousdenoisepass(ptresources& ptres) : res(ptres)
{
    auto dims = res.diffusecolortex->dims;
    dispatchxy = { divideup<8>(dims[0]), divideup<8>(dims[1]) };

    stdx::vecui2 diffradianceidx = { res.diffuseradiancetex[0].ex(), res.diffuseradiancetex[1].ex() };
    stdx::vecui2 specradianceidx = { res.specularradiancetex[0].ex(), res.specularradiancetex[1].ex() };
    stdx::vecui2 normaldepthidx = { res.normaldepthtex[0].ex(), res.normaldepthtex[1].ex() };
    stdx::vecui2 histlenddxyidx = { res.historylentex[0].ex(), res.historylentex[1].ex() };
    stdx::vecui2 hitpositionidx = { res.hitposition[0].ex(), res.hitposition[1].ex() };

    rt::denoiserparams denparams{ diffradianceidx, specradianceidx, normaldepthidx, histlenddxyidx, hitpositionidx, dims, res.sceneglobals };
    rt::postdenoiseparams postdenparams = { res.diffusecolortex.ex(), res.specbrdftex.ex(), diffradianceidx, specradianceidx, res.finalcolour, res.sceneglobals };

    denoiseparamsbuffer->create({ denparams });
    postdenoiseparamsbuffer->create({ postdenparams });
    denoiseparamsbuffer.ex() = denoiseparamsbuffer->createsrv().heapidx;
    postdenoiseparamsbuffer.ex() = postdenoiseparamsbuffer->createsrv().heapidx;
}

void atrousdenoisepass::operator()(deviceresources& devres)
{
    auto& globalres = globalresources::get();
    auto const& denoisepipeobjs = globalres.psomap().find("denoise")->second;

    devres.cmdlist->SetPipelineState(denoisepipeobjs.pso.Get());

    uavbarrierpass
    {
        res.historylentex[0], res.historylentex[1], res.momentstex[0], res.momentstex[1], res.diffusecolortex,
        res.diffuseradiancetex[0], res.diffuseradiancetex[1], res.specularradiancetex[0], res.specularradiancetex[1]
    }(devres);

    auto dispatchx = UINT(dispatchxy[0]);
    auto dispatchy = UINT(dispatchxy[1]);

    rt::denoiserootconsts denrootconsts = { denoiseparamsbuffer.ex() };
    for (uint32 i = 0; i < 1; ++i)
    {
        denrootconsts.passidx = i;

        devres.cmdlist->SetComputeRoot32BitConstants(0, 2, &denrootconsts, 0);
        devres.cmdlist->Dispatch(dispatchx, dispatchy, 1);

        uavbarrierpass{ res.diffuseradiancetex[0], res.diffuseradiancetex[1], res.specularradiancetex[0], res.specularradiancetex[1] }(devres);
    }

    auto const& postdenoisepipeobjs = globalres.psomap().find("postdenoise")->second;

    // todo : filter moments and illumination bilaterally before denosing(for low accumulated samples)
    devres.cmdlist->SetPipelineState(postdenoisepipeobjs.pso.Get());
    devres.cmdlist->SetComputeRoot32BitConstants(0, 1, &postdenoiseparamsbuffer.ex(), 0);
    devres.cmdlist->Dispatch(dispatchx, dispatchy, 1);
}

}