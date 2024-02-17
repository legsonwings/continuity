module;

#include <cassert>

export module activesample;

import <string>;
import <memory>;

import engine;
import graphics;

export enum class samples : int
{
	basic,
	sphfluid,
	num
};

export inline std::wstring sample_titles[int(samples::num)] = { L"Intro Sph fluid" };
export auto constexpr activesample = samples::sphfluid;

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

template <samples type>
std::unique_ptr<sample_base> create_instance(view_data const& data)
{
	assert(false && "shouldn't be here. create instance needs to be specailized for sample type.");
	return nullptr;
}

template <>
std::unique_ptr<sample_base> create_instance<samples::basic>(view_data const& data)
{
	return std::move(std::make_unique<basic_sample>());
}

}
