
#include "common.hlsli"

struct tonemapparams
{
	uint srctexture;
	uint desttexture;
};

[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<tonemapparams> paramsbuffer = ResourceDescriptorHeap[descriptorsidx.dispatchparams];
	tonemapparams params = paramsbuffer[0];

	Texture2D<float4> src = ResourceDescriptorHeap[params.srctexture];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[params.desttexture];

	float const color = src[dtid];
	
	// simple reinhard
	float4 const tonemapped = color / (1 + color);

	dest[dtid] = tonemapped;
}
