#include "common.hlsli"

struct accumparams
{
	uint currtexture;
	uint hdrcolourtex;
	uint accumcount;
};

[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<accumparams> paramsbuffer = ResourceDescriptorHeap[descriptorsidx.dispatchparams];
	accumparams params = paramsbuffer[0];

	RWTexture2D<float4> hdrout = ResourceDescriptorHeap[params.hdrcolourtex];
	RWTexture2D<float4> curr = ResourceDescriptorHeap[params.currtexture];

	float4 const prevc = curr[dtid];
	float4 const prevlinc = float4(linearcolour(prevc.xyz), prevc.w);
	float4 hdrcol = hdrout[dtid];

	// simple reinhard tonemap
	float4 currc = hdrcol / (1 + hdrcol);

	float4 accumcol = (params.accumcount * prevlinc + currc) / float(params.accumcount + 1);

	curr[dtid] = float4(srgbcolour(accumcol.xyz), accumcol.w);
}
