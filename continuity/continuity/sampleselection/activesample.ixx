export module activesample;

import engine;
import graphics;
import std.core;

export enum class samples : int
{
	basic,
	sphintro,
	num
};

export inline std::wstring sample_titles[int(samples::num)] = { L"Basic ", L"Intro SPH fluid " };
export auto constexpr activesample = samples::sphintro;

export class basic_sample : public sample_base
{
public:
	basic_sample() : sample_base(view_data{}) {};

	gfx::resourcelist load_assets_and_geometry() override { return {}; };
	void update(float dt) override {};
	void render(float dt) override {};
};

export namespace sample_creator
{

// implement a specialization for the sample
template <samples type>
std::unique_ptr<sample_base> create_instance(view_data const& data);

}
