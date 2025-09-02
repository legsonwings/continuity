
#include "common.hlsli"

struct genmipsparams
{
	uint srctexture;
	uint desttexture;
	uint linearsampler;
	float2 texelsize;
	uint srgb;
};

[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<genmipsparams> params = ResourceDescriptorHeap[descriptorsidx.dispatchparams];

	Texture2D<float4> src = ResourceDescriptorHeap[params[0].srctexture];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[params[0].desttexture];

	SamplerState linearsampler = SamplerDescriptorHeap[params[0].linearsampler];

	float2 const texcoords = params[0].texelsize * (dtid + 0.5f);

	// the samplers linear interpolation will mix the four pixel values to the new pixels color
	float4 const color = src.SampleLevel(linearsampler, texcoords, 0);

	// since uav writes do not support srgb we have to do linear->srgb conversion for the uav write
	dest[dtid] = params[0].srgb ? srgbcolour(color) : color;
}
