
export module sphintro;

import activesample;
import engine;
import graphics;
import std.core;

export namespace sphintro
{

class sphfluidintro : public sample_base
{
public:
	sphfluidintro() : sample_base(view_data{}) {};

	gfx::resourcelist load_assets_and_geometry() override { return {}; };
	void update(float dt) override {};
	void render(float dt) override {};
};

}

