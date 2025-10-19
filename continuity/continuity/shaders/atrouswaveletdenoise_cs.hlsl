
#include "common.hlsli"
#include "shared/raytracecommon.h"

ConstantBuffer<rt::denoiserootconsts> rootconsts: register(b0);

[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
	uint const passidx = rootconsts.passidx;
	StructuredBuffer<rt::denoiserparams> paramsbuff = ResourceDescriptorHeap[rootconsts.params];
	rt::denoiserparams params = paramsbuff[0];
	StructuredBuffer<rt::sceneglobals> scenebuf = ResourceDescriptorHeap[params.sceneglobals];

	// we ping pong between textures each frame, but now also ping pong for spatial passes
	// if frame and pass are both even or both odd then tex 1 has updated radiance
	uint oddframe = scenebuf[0].frameidx & 1;
	uint oddpass = passidx & 1;

	// these are only for radiance textures
	uint destidx = oddframe ^ oddpass;
	uint srcidx = destidx ^ 1u;

	uint histidx = oddframe;
	uint curridx = oddframe ^ 1u;

	RWTexture2D<float4> diffsrc = ResourceDescriptorHeap[params.difftex[srcidx]];
	RWTexture2D<float4> diffdest = ResourceDescriptorHeap[params.difftex[destidx]];
	RWTexture2D<float4> specsrc = ResourceDescriptorHeap[params.spectex[srcidx]];
	RWTexture2D<float4> specdest = ResourceDescriptorHeap[params.spectex[destidx]];

	RWTexture2D<float4> normaldepth = ResourceDescriptorHeap[params.normaldepthtex[curridx]];
	RWTexture2D<float2> currhistorylendepthdxtex = ResourceDescriptorHeap[params.historylentex[curridx]];
	RWTexture2D<float4> hitpostex = ResourceDescriptorHeap[params.hitposition[curridx]];

	float kernel[5] = { 1 / 16.0, 1 / 4.0, 3 / 8.0, 1 / 4.0, 1 / 16.0 };

	float nphi = 32.0;
	float dphi = 1.0;
	float cphi = 1.0;

	float cd = normaldepth[dtid].w;
	float3 cn = normaldepth[dtid].xyz;
	float3 diffcc = diffsrc[dtid].xyz;
	float3 speccc = specsrc[dtid].xyz;

	float stepwidth = 1u << passidx;

	cphi = cphi * pow(2, -float(passidx));

	float ddxy = currhistorylendepthdxtex[dtid].y;
	float const gaussiankernel[2][2] = { { 1.0 / 4.0, 1.0 / 8.0 }, { 1.0 / 8.0, 1.0 / 16.0 } };
	float const historylen = currhistorylendepthdxtex[dtid].x;

	float2 sumdiffspecv = (float2)0;
	if(historylen < 4.0)
	{
		// spatial variance if low history
		[unroll] for (int y = -1; y <= 1; ++y)
			[unroll] for (int x = -1; x <= 1; ++x)
			{
				int2 p = dtid + int2(x, y);
				float2 diffspecv = float2(diffsrc[p].w, specsrc[p].w);
				sumdiffspecv += diffspecv * gaussiankernel[abs(x)][abs(y)];
			}
	}
	else
	{
		sumdiffspecv = float2(diffsrc[dtid].w, specsrc[dtid].w);
	}

	// todo : needs normalization when the kernel is centered at the edges
	float2 const diffspecphi = cphi * sqrt(max(0, sumdiffspecv));

	// plane distance normal
	float pdn = 1.0 / (1.0 + stepwidth * ddxy * cd);
	float3 hitp = hitpostex[dtid].xyz;
	float2 diffspecsumw = (float2)0;
	float2x4 diffspecsum = (float2x4)0;

	// todo : may have to weigh center pixel as 1 to avoid any edge issues
	for (int i = -2; i <= 2; i++)
		for (int j = -2; j <= 2; j++)
		{
			int2 p = dtid + int2(i, j) * stepwidth;

			// todo : ignore out of bounds samples, proabaly should normalize weights
			if (all(uint2(p) < params.dims))
			{
				float3 pn = normaldepth[p].xyz;
				float nw = pow(saturate(dot(cn, pn)), nphi);
				
				float3 phitp = hitpostex[p].xyz;
				float dw = saturate(1.0 - (abs(planedist(phitp, hitp, cn)) * pdn));

				float4 diffpc = diffsrc[p];
				float4 specpc = specsrc[p];
				float3x2 cdiff = float3x2(diffcc - diffpc.xyz, speccc- specpc.xyz);

				float2 diffspecdist2 = float2(dot(cdiff[0], cdiff[0]), dot(cdiff[1], cdiff[1]));
				float2 diffspecc_w = min(exp(-diffspecdist2 / (diffspecphi + 1e-10f)), 1.0);
				float2 diffspecw = nw * dw * diffspecc_w * kernel[abs(i)] * kernel[abs(j)];

				diffspecsumw += diffspecw;
				diffspecsum[0] += float4(diffspecw[0].xxx, diffspecw[0] * diffspecw[0]) * diffpc;
				diffspecsum[1] += float4(diffspecw[1].xxx, diffspecw[1] * diffspecw[1]) * specpc;
			}
		}

	float2 const diffspecsumw2rcp = rcp(diffspecsumw * diffspecsumw);

	diffspecsum[0] *= float4((diffspecsumw2rcp[0] * diffspecsumw[0]).xxx, diffspecsumw2rcp[0]);
	diffspecsum[1] *= float4((diffspecsumw2rcp[1] * diffspecsumw[1]).xxx, diffspecsumw2rcp[1]);

	diffdest[dtid] = diffspecsum[0];
	specdest[dtid] = diffspecsum[1];
}
