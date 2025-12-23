module;

// there are three modes atm
// reference : multiple spp, no denoising, simple temporal accumulation, unstable under motion
// high quality : multiple spp, atrous wavelet denoising, simple temporal accumulation, unstable under motion
// real time : single spp, svgf based temporal and spatial denoising, stable under motion

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
static std::uniform_real_distribution<float> distuf(0.f, 1.f);
static std::uniform_real_distribution<float> distsf(-0.5f, 0.5f);
static std::uniform_int_distribution<uint32> distu(0, std::numeric_limits<uint32>::max());

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::pathtrace>(view_data const& data)
{
	return std::move(std::make_unique<pathtrace>(data));
}

}

// raytracing stuff
static constexpr char const* raygenshadername = "raygenshader";

pathtrace::pathtrace(view_data const& viewdata) : sample_base(viewdata)
{
    camera.Init({ -600.f, 180, 0 }, 1.57f);
	camera.SetMoveSpeed(80.0f);

    scenedata = {};
    scenedata.numbounces = 3;
    scenedata.ggxsampleridx = 1;
}

gfx::resourcelist pathtrace::create_resources(gfx::renderer& r)
{
    auto& globalres = gfx::globalresources::get();

    gfx::resourcelist res;

    gfx::raytraceshaders rtshaders;
    rtshaders.raygen = raygenshadername;

    auto& raytracepipeline_objs = globalres.addraytracingpso("pathtrace", "pathtrace_rs.cso", rtshaders, 0, 0);

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

    viewglobalsbuffer->create(2);
    sceneglobalsbuffer->create(1);

    viewglobalsbuffer.ex() = viewglobalsbuffer->createsrv().heapidx;
    sceneglobalsbuffer.ex() = sceneglobalsbuffer->createsrv().heapidx;

    // default initialize current frame view, so it gets copied to previous frame buffer correctly
    rt::viewglobals& currview = viewglobalsbuffer.get()[1];
    currview = {};
    currview.view = currview.viewproj = currview.invviewproj = utils::to_matrix4x4(matrix::Identity);

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

    // now build shader tables
    // only one records for ray gen and miss shader tables
    raygenshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
    raygenshadertable.addrecord(raygenshaderid);

	ptsettingsbuffer->create({ ptsettings });
    ptsettingsbuffer.ex() = ptsettingsbuffer->createsrv().heapidx;

    rt::dispatchparams dispatch_params;
    dispatch_params.posbuffer = posbuffer.createsrv().heapidx;
    dispatch_params.texcoordbuffer = texcoordbuffer.createsrv().heapidx;
    dispatch_params.tbnbuffer = tbnbuffer.createsrv().heapidx;
    dispatch_params.primitivematerialsbuffer = materialsbuffer.createsrv().heapidx;
    dispatch_params.viewglobals = viewglobalsbuffer.ex();
    dispatch_params.sceneglobals = sceneglobalsbuffer.ex();
    dispatch_params.accelerationstruct = tlas.createsrv().heapidx;
    dispatch_params.dims = { viewdata.width, viewdata.height };
    dispatch_params.indexbuffer = indexbuffer.createsrv().heapidx;
    dispatch_params.ptsettings = ptsettingsbuffer.ex();
    dispatch_params.normaldepthtex = { r.normaldepthtex[0].ex(), r.normaldepthtex[1].ex() };
    dispatch_params.historylentex = { r.historylentex[0].ex(), r.historylentex[1].ex() };
    dispatch_params.diffcolortex = r.diffusecolortex.ex();
    dispatch_params.specbrdftex = r.specbrdftex.ex();
    dispatch_params.diffradiancetex = { r.diffuseradiancetex[0].ex(), r.diffuseradiancetex[1].ex() };
    dispatch_params.specradiancetex = { r.specularradiancetex[0].ex(), r.specularradiancetex[1].ex() };
    dispatch_params.rtoutput = r.hdrrendertarget.ex();
    dispatch_params.hitposition = { r.hitposition[0].ex(), r.hitposition[1].ex() };

    dispatchparams.create({ dispatch_params });

    rootdescs.rootdesc = dispatchparams.createsrv().heapidx;

    return res;
}

void pathtrace::render(float dt, gfx::renderer& renderer)
{
    viewglobalsbuffer.get()[0] = viewglobalsbuffer.get()[1];

    auto currviewmatrix = matrix(camera.GetViewMatrix());
    accumdirty = accumdirty || (prevviewmatrix != currviewmatrix);
    if (accumdirty)
    {
        accumdirty = false;
        renderer.clearaccumcount();
    }

    auto lightdir = stdx::vec3{ -1.0f, -1.0f, -0.0f }.normalized();
    auto& globalres = gfx::globalresources::get();

    rt::viewglobals camviewinfo;
    scenedata.matbuffer = globalres.materialsbuffer_idx();
    scenedata.viewdirshading = false; // todo : this shouldn't be in scenedata
    scenedata.lightdir = lightdir;
    scenedata.lightluminance = 6;
    scenedata.frameidx = framecount;
    scenedata.seed = distuf(re);
    scenedata.aoradius = 50;
    scenedata.enableao = 0;
    scenedata.envtex = renderer.envtexidx;

    auto viewmat = matrix(camera.GetViewMatrix());
    auto viewproj = viewmat * matrix(camera.GetProjectionMatrix());
    camviewinfo.viewpos = camera.GetCurrentPosition();
    camviewinfo.view = utils::to_matrix4x4(viewmat);
    camviewinfo.viewproj = utils::to_matrix4x4(viewproj);
    camviewinfo.invviewproj = utils::to_matrix4x4(viewproj.Invert());

    auto const jitter = stdx::vec2{ distsf(re), distsf(re) };

    // jitter is probably only needed if using taa
    camviewinfo.jitter = ptsettings.camjitter ? jitter : stdx::vec2{};

    viewglobalsbuffer->update({ camviewinfo }, 1);
    sceneglobalsbuffer->update({ scenedata });

    auto& cmdlist = renderer.deviceres().cmdlist;
    auto const& pipelineobjects = globalres.psomap().find("pathtrace")->second;

    ID3D12DescriptorHeap* heaps[] = { renderer.resheap.d3dheap.Get(), renderer.sampheap.d3dheap.Get() };
    cmdlist->SetDescriptorHeaps(_countof(heaps), heaps);
    cmdlist->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    cmdlist->SetPipelineState1(pipelineobjects.pso_raytracing.Get());

    gfx::raytrace rt;
    for (auto i : stdx::range(ptsettings.spp))
    {
        rootdescs.sampleidx = i;
        cmdlist->SetComputeRoot32BitConstants(0, 2, &rootdescs, 0);
        rt.dispatchrays(raygenshadertable, {}, {}, pipelineobjects.pso_raytracing.Get(), viewdata.width, viewdata.height);
        gfx::uav_barrier(
                        *cmdlist.Get(), renderer.hdrrendertarget, renderer.diffusecolortex, renderer.diffuseradiancetex[0], renderer.diffuseradiancetex[1], renderer.specularradiancetex[0], renderer.specularradiancetex[1],
                        renderer.historylentex[0], renderer.historylentex[1], renderer.hitposition[0], renderer.hitposition[1]
                        );
    }

    renderer.viewglobals = viewglobalsbuffer.ex();
    renderer.sceneglobals = sceneglobalsbuffer.ex();
	renderer.ptsettings = ptsettingsbuffer.ex();
    renderer.spatialdenoise = !ptsettings.simpleaccum;

    prevviewmatrix = currviewmatrix;

    framecount++;
}

void pathtrace::on_key_up(unsigned key)
{
    sample_base::on_key_up(key);

    if (key == 'K')
        scenedata.numbounces++;
    if (key == 'L')
        scenedata.numbounces = std::max(scenedata.numbounces, 1u) - 1;

    if (key == 'B')
    {
        accumdirty = true;
        scenedata.ggxsampleridx = (++scenedata.ggxsampleridx) % 3;
    }

    if (key >= '0' && key <= '9')
        scenedata.viewmode = key - '0';
}
