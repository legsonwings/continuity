
#include "common.hlsli"
#include "shared/raytracecommon.h"

struct postdenoiseconsts
{
	uint32 params;
};

ConstantBuffer<postdenoiseconsts> rootconsts: register(b0);

[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<rt::postdenoiseparams> paramsbuff = ResourceDescriptorHeap[rootconsts.params];
	rt::postdenoiseparams params = paramsbuff[0];
	StructuredBuffer<rt::sceneglobals> sceneglobals = ResourceDescriptorHeap[params.sceneglobals];

	uint idx = (sceneglobals[0].frameidx & 1u);

	RWTexture2D<float4> diffcolor = ResourceDescriptorHeap[params.diffcolor];
	RWTexture2D<float4> specbrdfest = ResourceDescriptorHeap[params.specbrdf];
	RWTexture2D<float4> diff = ResourceDescriptorHeap[params.diffradiance[idx]];
	RWTexture2D<float4> spec = ResourceDescriptorHeap[params.specradiance[idx]];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[params.outputrt];

	float3 colour = diff[dtid].xyz * max(diffcolor[dtid].xyz, 1e-10) + spec[dtid].xyz * max(specbrdfest[dtid].xyz, 1e-10);

	// simple reinhard tonemap
	colour = colour / (1.0 + colour);

	dest[dtid] = float4(srgbcolour(colour), saturate(diffcolor[dtid].w));
}
