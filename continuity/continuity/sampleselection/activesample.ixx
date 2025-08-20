export module activesample;

import engine;
import graphicscore;
import std;

export enum class samples : int
{
	basic,
	playground,
	sphintro,
	sphgpu,
	raytrace,
	pathtrace,
	num
};

export inline std::wstring sample_titles[int(samples::num)] = { L"Basic ", L"Playground", L"Intro SPH fluid ", L"Sph Gpu fluid", L"raytrace", L"pathtrace" };
export auto constexpr activesample = samples::pathtrace;

namespace gfx { class renderer; }

export class basic_sample : public sample_base
{
public:
	basic_sample() : sample_base(view_data{}) {};

	gfx::resourcelist create_resources(gfx::deviceresources&) override { return {}; };
	void update(float dt) override {};
	void render(float dt, gfx::renderer&) override {};
};

export namespace sample_creator
{

// implement a specialization for the sample
template <samples type>
std::unique_ptr<sample_base> create_instance(view_data const& data);

}
