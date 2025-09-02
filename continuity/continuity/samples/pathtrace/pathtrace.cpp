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

static std::random_device rd;
static std::mt19937 re(rd());
static std::uniform_real_distribution<float> dist(0.f, 1.f);
static std::uniform_int_distribution<uint32> distu(0, std::numeric_limits<uint32>::max());

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
	camera.Init({ 0.f, 0.f, 0 });
	camera.SetMoveSpeed(200.0f);

    scenedata = {};
    scenedata.numbounces = 1;
    scenedata.numindirectrays = 2;
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

    auto const& vertices = model.vertices();
    auto const& indices = model.indices();
    indexbuffer.create(indices);
    posbuffer.create(vertices.positions);
    texcoordbuffer.create(vertices.texcoords);
    tbnbuffer.create(vertices.tbns);

    std::vector<uint32> positionindices;
    positionindices.reserve(indices.size());
    for (auto const& idx : indices)
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
    dispatch_params.texcoordbuffer = texcoordbuffer.createsrv().heapidx;
    dispatch_params.tbnbuffer = tbnbuffer.createsrv().heapidx;
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
    auto lightpos0 = stdx::vec3{ 600, 600, 100 };
    auto lightpos1 = stdx::vec3{ 600, 600, -100 };
    auto lightfocus = stdx::vec3{ -800, 450, 0 };
    auto lightdir0 = (lightfocus - lightpos0).normalized();
    auto lightdir1 = (lightfocus - lightpos1).normalized();

    auto& globalres = gfx::globalresources::get();

    rt::viewglobals camviewinfo;
    scenedata.matbuffer = globalres.materialsbuffer_idx();
    scenedata.viewdirshading = false; // todo : this shouldn't be in scenedata
    scenedata.lightpos0 = lightpos0;
    scenedata.lightdir0 = lightdir0;
    scenedata.lightluminance0 = 6;
    scenedata.lightpos1 = lightpos1;
    scenedata.lightdir1 = lightdir1;
    scenedata.lightluminance1 = 6;
    scenedata.frameidx = framecount++;
    scenedata.seed = dist(re);
    scenedata.seedu = distu(re);

    auto viewproj = matrix(camera.GetViewMatrix() * camera.GetProjectionMatrix());
    camviewinfo.viewpos = camera.GetCurrentPosition();
    camviewinfo.viewproj = utils::to_matrix4x4(viewproj);
    camviewinfo.invviewproj = utils::to_matrix4x4(viewproj.Invert());

    viewglobalsbuffer.update({ camviewinfo });
    sceneglobalsbuffer.update({ scenedata });

    auto& cmdlist = renderer.deviceres().cmdlist;
    auto const& pipelineobjects = globalres.psomap().find("pathtrace")->second;

    ID3D12DescriptorHeap* heaps[] = { renderer.resheap.d3dheap.Get(), renderer.sampheap.d3dheap.Get() };
    cmdlist->SetDescriptorHeaps(_countof(heaps), heaps);
    cmdlist->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    cmdlist->SetComputeRoot32BitConstants(0, 1, &rootdescs, 0);
    cmdlist->SetPipelineState1(pipelineobjects.pso_raytracing.Get());

    gfx::raytrace rt;
    rt.dispatchrays(raygenshadertable, missshadertable, hitgroupshadertable, pipelineobjects.pso_raytracing.Get(), viewdata.width, viewdata.height);
    rt.copyoutputtorendertarget(cmdlist.Get(), raytracingoutput, renderer.rendertarget.d3dresource.Get());
}

void pathtrace::on_key_up(unsigned key)
{
    sample_base::on_key_up(key);

    if (key == 'K')
        scenedata.numbounces++;
    if (key == 'L')
        scenedata.numbounces = std::max(scenedata.numbounces, 1u) - 1;
    if (key == 'O')
        scenedata.numindirectrays++;
    if (key == 'P')
        scenedata.numindirectrays = std::max(scenedata.numbounces, 1u) - 1;

    if (key >= '0' && key <= '9')
        scenedata.viewmode = key - '0';
}
