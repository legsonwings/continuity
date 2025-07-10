module;

#include "simplemath/simplemath.h"

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

gfx::resourcelist playground::create_resources()
{
    using gfx::bodyparams;

    auto& globalres = gfx::globalresources::get();

    viewglobalsbuffer.create();
    sceneglobalsbuffer.create();

    gfx::material mat;
    mat.basecolour = stdx::vec4{ 0.0f, 1.0, 0.0, 1.0f };
    mat.metallic = 0u;
    mat.roughness = 0.25f;
    mat.reflectance = 0.5f;

    auto matid = globalres.addmat(mat);

    // no need to write objdesc as it is written by body
    gfx::rootdescriptors rootdescs;
    rootdescs.viewglobalsdesc = viewglobalsbuffer.createsrv().heapidx;
    rootdescs.sceneglobalsdesc = sceneglobalsbuffer.createsrv().heapidx;

    // since these use static vertex buffers, just send 0 as maxverts
    auto &model = models.emplace_back(gfx::model("models/sponza/sponza.obj", true), bodyparams{ 0, 1, "instanced", matid } );
    model.rootdescriptors() = rootdescs;

    gfx::resourcelist res;
    for (auto b : stdx::makejoin<gfx::bodyinterface>(models)) { stdx::append(b->create_resources(), res); };
    return res;
}

void playground::update(float dt)
{
    sample_base::update(dt);
    for (auto b : stdx::makejoin<gfx::bodyinterface>(models)) b->update(dt);

    viewglobals viewdata;
    sceneglobals scenedata;

    auto& globalres = gfx::globalresources::get();

    viewdata.campos = camera.GetCurrentPosition();
    viewdata.viewproj = utils::to_matrix4x4((globalres.view().view * globalres.view().proj));
    scenedata.matbuffer = globalres.materialsbuffer_idx();
    scenedata.viewdirshading = viewdirshading;

    viewglobalsbuffer.update({ viewdata });
    sceneglobalsbuffer.update({ scenedata });
}

void playground::render(float dt)
{
    for (auto b : stdx::makejoin<gfx::bodyinterface>(models)) b->render(dt, { false });
}

void playground::on_key_up(unsigned key)
{
    if (key == '1')
    {
        viewdirshading = 1 - viewdirshading;
    }

    sample_base::on_key_up(key);
}
