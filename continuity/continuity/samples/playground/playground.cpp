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
	camera.SetMoveSpeed(100.0f);
}

gfx::resourcelist playground::create_resources()
{
    using geometry::cube;
    using geometry::sphere;
    using gfx::bodyparams;
    using gfx::material;

    auto& globalres = gfx::globalresources::get();
    auto& globals = globalres.cbuffer().data();

    // initialize lights
    globals.numdirlights = 1;
    globals.numpointlights = 2;

    globals.ambient = { 0.1f, 0.1f, 0.1f, 1.0f };
    globals.lights[0].direction = vector3{ 0.3f, -0.27f, 0.57735f }.Normalized();
    globals.lights[0].color = { 0.2f, 0.2f, 0.2f };

    globals.lights[1].position = { -15.f, 15.f, -15.f };
    globals.lights[1].color = { 1.f, 1.f, 1.f };
    globals.lights[1].range = 40.f;

    globals.lights[2].position = { 15.f, 15.f, -15.f };
    globals.lights[2].color = { 1.f, 1.f, 1.f };
    globals.lights[2].range = 40.f;

    globalres.cbuffer().updateresource();

    //std::vector<material> materials_data(1u);

    //materials_data[0].colour = stdx::vec4{ 0.0f, 1.0, 0.0, 1.0f };
    //materials_data[0].metallic = 1u;
    //materials_data[0].roughness = 0.25f;
    //materials_data[0].reflectance = 0.5f;

    //materialids.create(material_idsdata);
    //materials.create(materials_data);

    // since these use static vertex buffers, just send 0 as maxverts
    models.emplace_back(gfx::model("models/spot.obj", true), bodyparams{ 0, 1, "instanced" } );

    gfx::resourcelist res;
    for (auto b : stdx::makejoin<gfx::bodyinterface>(models)) { stdx::append(b->create_resources(), res); };
    return res;
}

void playground::update(float dt)
{
    sample_base::update(dt);
    for (auto b : stdx::makejoin<gfx::bodyinterface>(models)) b->update(dt);
}

void playground::render(float dt)
{
    for (auto b : stdx::makejoin<gfx::bodyinterface>(models)) b->render(dt, { false });
}
