module;

#include "simplemath/simplemath.h"
#include "thirdparty/d3dx12.h"
#include "thirdparty/dxhelpers.h"
#include "shared/raytracecommon.h"

module pathtrace;

import stdx;
import vec;
import std;
import activesample;

using matrix = DirectX::SimpleMath::Matrix;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::pathtrace>(view_data const& data)
{
	return std::move(std::make_unique<pathtrace>(data));
}

}

#define namkaran(d3dobject) { d3dobject->SetName(utils::strtowstr(#d3dobject).c_str()); }
#define namkaranres(gfxresource) { gfxresource.d3dresource->SetName(utils::strtowstr(#gfxresource).c_str()); }

// raytracing stuff
static constexpr char const* hitgroupname = "trianglehitgroup";
static constexpr char const* raygenshadername = "raygenshader";
static constexpr char const* closesthitshadername = "closesthitshader_triangle";
static constexpr char const* missshadername = "missshader";

pathtrace::pathtrace(view_data const& viewdata) : sample_base(viewdata)
{
	camera.Init({ 0.f, 0.f, -500.f });
	camera.SetMoveSpeed(200.0f);
}

gfx::resourcelist pathtrace::create_resources(gfx::deviceresources& deviceres)
{
    auto& globalres = gfx::globalresources::get();

    gfx::resourcelist res;

    gfx::raytraceshaders rtshaders;
    rtshaders.raygen = raygenshadername;
    rtshaders.miss = missshadername;
    rtshaders.tri_hitgrp.name = hitgroupname;
    rtshaders.tri_hitgrp.closesthit = closesthitshadername;

    auto& raytracepipeline_objs = globalres.addraytracingpso("pathtrace", "pathtrace_rs.cso", rtshaders);

    model = gfx::model("models/sponza/sponza.obj", res);
    gfx::geometryopacity const opacity = gfx::geometryopacity::opaque;
    gfx::blasinstancedescs instancedescs;

    indexbuffer.create(model.indices());
    posbuffer.create(model.vertices().positions);
        
    //texcoordbuffer.create(model.vertices().texcoords);
    //tbnbuffer.create(model.vertices().tbns);

    std::vector<uint32> positionindices;
    positionindices.reserve(model.indices().size());
    for (auto const& idx : model.indices())
        positionindices.emplace_back(idx.pos);

    gfx::structuredbuffer<uint32, gfx::accesstype::both> posindexbuffer;

    posindexbuffer.create(positionindices);
    res.push_back(posindexbuffer.d3dresource);

    materialsbuffer.create(model.materials());
        
    viewglobalsbuffer.create(1);
    sceneglobalsbuffer.create(1);

    // cannot use stdx::join because ComPtr is too smart for its own good
    for (auto r : triblas.build(instancedescs, opacity, posbuffer, posindexbuffer))
        res.push_back(r);

    for (auto r : tlas.build(instancedescs))
        res.push_back(r);

    ComPtr<ID3D12StateObjectProperties> stateobjectproperties;
    ThrowIfFailed(raytracepipeline_objs.pso_raytracing.As(&stateobjectproperties));
    auto* stateobjectprops = stateobjectproperties.Get();

    // get shader id's
    void* raygenshaderid = stateobjectprops->GetShaderIdentifier(utils::strtowstr(raygenshadername).c_str());
    void* missshaderid = stateobjectprops->GetShaderIdentifier(utils::strtowstr(missshadername).c_str());
    void* hitgroupshaderids_trianglegeometry = stateobjectprops->GetShaderIdentifier(utils::strtowstr(hitgroupname).c_str());

    // now build shader tables
    // only one records for ray gen and miss shader tables
    raygenshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
    raygenshadertable.addrecord(raygenshaderid);

    missshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
    missshadertable.addrecord(missshaderid);

    // triangle record for hitgroup shader table
    hitgroupshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
    hitgroupshadertable.addrecord(hitgroupshaderids_trianglegeometry);

    raytracingoutput.create(DXGI_FORMAT_R8G8B8A8_UNORM, stdx::vecui2{ viewdata.width, viewdata.height }, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    rt::dispatchparams dispatch_params;
    dispatch_params.posbuffer = posbuffer.createsrv().heapidx;
    //dispatch_params.texcoordbuffer = texcoordbuffer.createsrv().heapidx;
    //dispatch_params.tbnbuffer = tbnbuffer.createsrv().heapidx;
    dispatch_params.primitivematerialsbuffer = materialsbuffer.createsrv().heapidx;
    dispatch_params.viewglobals = viewglobalsbuffer.createsrv().heapidx;
    dispatch_params.sceneglobals = sceneglobalsbuffer.createsrv().heapidx;
    dispatch_params.accelerationstruct = tlas.createsrv().heapidx;
    dispatch_params.rtoutput = raytracingoutput.createuav().heapidx;
    dispatch_params.indexbuffer = indexbuffer.createsrv().heapidx;

    dispatchparams.create({ dispatch_params });

    rootdescs.rootdesc = dispatchparams.createsrv().heapidx;

    return res;
}

void pathtrace::render(float dt, gfx::renderer& renderer)
{
    auto lightpos = stdx::vec3{ 800, 800, 0 };
    auto lightfocus = stdx::vec3{ -800, 450, 0 };
    auto lightdir = (lightfocus - lightpos).normalized();

    auto& globalres = gfx::globalresources::get();

    rt::viewglobals camviewinfo;
    rt::sceneglobals scenedata;
    scenedata.matbuffer = globalres.materialsbuffer_idx();
    scenedata.viewdirshading = false; // todo : this shouldn't be in scenedata
    scenedata.lightdir = lightdir;
    scenedata.lightluminance = 6;

    camviewinfo.viewpos = camera.GetCurrentPosition();
    camviewinfo.invviewproj = utils::to_matrix4x4(matrix(camera.GetViewMatrix() * camera.GetProjectionMatrix()).Invert());

    viewglobalsbuffer.update({ camviewinfo });
    sceneglobalsbuffer.update({ scenedata });

    auto& cmdlist = renderer.deviceres().cmdlist;
    auto const& pipelineobjects = globalres.psomap().find("pathtrace")->second;

    cmdlist->SetDescriptorHeaps(1, globalres.resourceheap().d3dheap.GetAddressOf());
    cmdlist->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    cmdlist->SetComputeRoot32BitConstants(0, 1, &rootdescs, 0);
    cmdlist->SetPipelineState1(pipelineobjects.pso_raytracing.Get());

    gfx::raytrace rt;
    rt.dispatchrays(raygenshadertable, missshadertable, hitgroupshadertable, pipelineobjects.pso_raytracing.Get(), viewdata.width, viewdata.height);
    rt.copyoutputtorendertarget(cmdlist.Get(), raytracingoutput, renderer.rendertarget.d3dresource.Get());
}