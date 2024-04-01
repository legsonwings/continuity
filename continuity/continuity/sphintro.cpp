module;

#include "simplemath/simplemath.h"

module sphintro;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::sphintro>(view_data const& data)
{
	return std::move(std::make_unique<sphfluidintro>(data));
}

}

using vector3 = DirectX::SimpleMath::Vector3;

sphfluidintro::sphfluidintro(view_data const& viewdata) : sample_base(viewdata)
{
	camera.Init({ 0.f, 0.f, -43.f });
	camera.SetMoveSpeed(10.0f);
}

gfx::resourcelist sphfluidintro::create_resources()
{
    using geometry::cube;
    using geometry::sphere;
    using gfx::bodyparams;
    using gfx::material;

    using namespace DirectX;

    auto const& globalres = gfx::globalresources::get();
    auto const& redmaterial = gfx::globalresources::get().mat("ball");
    auto& globals = gfx::globalresources::get().cbuffer().data();

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

    gfx::globalresources::get().cbuffer().updateresource();

    // since these use static vertex buffers, just send 0 as maxverts
    boxes.emplace_back(cube{ vector3{0.f, 0.f, 0.f}, vector3{40.f} }, &cube::vertices_flipped, &cube::instancedata, bodyparams{ 0, 1, "instanced" });
    particles.emplace_back(sphere{ vector3{0.0f, 0.0f, 0.0f}, 8.0f }, bodyparams{ 0, 1, "instanced" });

    gfx::resourcelist res;
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, particles)) { stdx::append(b->create_resources(), res); };
    return res;
}

void sphfluidintro::update(float dt)
{
    sample_base::update(dt);
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, particles)) b->update(dt);

}

void sphfluidintro::render(float dt)
{
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, particles))
        b->render(dt, { false });
}