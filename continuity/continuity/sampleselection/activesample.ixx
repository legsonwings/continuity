export module activesample;

import engine;
import graphics;
import std.core;

export enum class samples : int
{
	basic,
	sphintro,
	sphgpu,
	num
};

export inline std::wstring sample_titles[int(samples::num)] = { L"Basic ", L"Intro SPH fluid ", L"Sph Gpu fluid"};
export auto constexpr activesample = samples::sphgpu;

export class basic_sample : public sample_base
{
public:
	basic_sample() : sample_base(view_data{}) {};

	gfx::resourcelist create_resources() override { return {}; };
	void update(float dt) override {};
	void render(float dt) override {};
};

export namespace sample_creator
{

// implement a specialization for the sample
template <samples type>
std::unique_ptr<sample_base> create_instance(view_data const& data);

}
