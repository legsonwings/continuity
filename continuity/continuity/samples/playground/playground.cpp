module;

#include "simplemath/simplemath.h"
#include <thirdparty/d3dx12.h>

module playground;

import stdx;
import vec;
import std;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::playground>(view_data const& data)
{
	return std::move(std::make_unique<playground>(data));
}

}

using namespace DirectX;

playground::playground(view_data const& viewdata) : sample_base(viewdata)
{
	camera.Init({ 0.f, 0.f, -30.f });
	camera.SetMoveSpeed(200.0f);
}

gfx::resourcelist playground::create_resources(gfx::deviceresources& deviceres)
{
    using gfx::bodyparams;

    auto& globalres = gfx::globalresources::get();

    viewglobalsbuffer.create(2);
    sceneglobalsbuffer.create(2);

    gfx::material mat;
    mat.basecolour = stdx::vec4{ 0.0f, 1.0, 0.0, 1.0f };
    mat.metallic = 0u;
    mat.roughness = 0.25f;
    mat.reflectance = 0.5f;

    auto matid = globalres.addmat(mat);

    gfx::resourcelist res;

    // since these use static vertex buffers, just send 0 as maxverts
    auto &model = models.emplace_back(gfx::model("models/sponza/sponza.obj", res), bodyparams{ 0, 1, matid } );

    for (auto b : stdx::makejoin<gfx::bodyinterface>(models)) { stdx::append(b->create_resources(), res); };

	rootdescs.dispatchparams = models[0].descriptorsindex();
    rootdescs.viewglobalsdesc = viewglobalsbuffer.createsrv().heapidx;
    rootdescs.sceneglobalsdesc = sceneglobalsbuffer.createsrv().heapidx;

    return res;
}

void playground::update(float dt)
{
    sample_base::update(dt);
    for (auto b : stdx::makejoin<gfx::bodyinterface>(models)) b->update(dt);
}

void playground::render(float dt, gfx::renderer& renderer)
{
    auto lightpos = vector3(800, 800, 0);
    auto lightfocus = vector3(-800, 450, 0);
    auto lightdir = (lightfocus - lightpos).Normalized();

    auto& globalres = gfx::globalresources::get();

    viewglobals camviewinfo, lightviewinfo;
    sceneglobals scenedata;
    scenedata.shadowmap = renderer.shadowmapsrv.heapidx;
    scenedata.matbuffer = globalres.materialsbuffer_idx();
    scenedata.viewdirshading = viewdirshading;
    scenedata.lightdir = stdx::vec3{ lightdir[0], lightdir[1], lightdir[2] };
    scenedata.lightluminance = 6;

    matrix lightviewmatrix = XMMatrixLookAtLH(lightpos, lightfocus, vector3::UnitY);

    float aspectr = float(viewdata.width) / viewdata.height;
    float w = 4000;
    float h = w / aspectr;

    matrix lightorthoproj = XMMatrixOrthographicLH(w, h, 5000, 0.05f);
    
    camviewinfo.viewpos = camera.GetCurrentPosition();
    camviewinfo.viewproj = utils::to_matrix4x4(matrix(camera.GetViewMatrix() * camera.GetProjectionMatrix()).Transpose());

    lightviewinfo.viewpos = { lightpos[0], lightpos[1], lightpos[2] };
    lightviewinfo.viewproj = utils::to_matrix4x4((lightviewmatrix * lightorthoproj).Transpose());

    viewglobalsbuffer.update({ camviewinfo, lightviewinfo });
    sceneglobalsbuffer.update({ scenedata });

    auto const shadowpipelineobjs = globalres.psomap().find("instanced_depthonly")->second;
    auto const mainpipelineobjs = globalres.psomap().find("instanced")->second;

    auto rthandle = renderer.rtheap.cpuhandle(renderer.rtview.heapidx);
    auto dthandle = renderer.dtheap.cpuhandle(renderer.dtview.heapidx);
    auto shadowdthandle = renderer.dtheap.cpuhandle(renderer.shadowmapdtv.heapidx);
    auto viewportsize = stdx::vecui2{ viewdata.width, viewdata.height };

    std::vector<uint32> rootdescsv = gfx::aggregatetovector(rootdescs);

    // shadow map probably should lower resolution than main rt
    gfx::pipelinestate shadowps{ "instanced_depthonly", viewportsize, {}, shadowdthandle };
    gfx::pipelinestate mainps{ "instanced", viewportsize, rthandle, dthandle };

    stdx::vecui3 dispatch = { gfx::divideup<85>(models[0].numprims()), 1, 1 };

    renderer.dispatchmesh(dispatch, shadowps, rootdescsv);

    auto barrier_shadowmap = CD3DX12_RESOURCE_BARRIER::Transition(renderer.shadowmap.d3dresource.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
 
    auto& cmdlist = renderer.deviceres().cmdlist;
    cmdlist->ResourceBarrier(1, &barrier_shadowmap);

    renderer.dispatchmesh(dispatch, mainps, rootdescsv);

    auto revbarrier_shadowmap = gfx::reversetransition(barrier_shadowmap);
    cmdlist->ResourceBarrier(1, &revbarrier_shadowmap);
}

void playground::on_key_up(unsigned key)
{
    if (key == '1')
        viewdirshading = 1 - viewdirshading;

    sample_base::on_key_up(key);
}
