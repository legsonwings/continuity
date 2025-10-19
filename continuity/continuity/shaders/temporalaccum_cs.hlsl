#include "common.hlsli"
#include "shared/raytracecommon.h"

void simpleaccum(uint2 dtid, rt::accumparams params)
{
	RWTexture2D<float4> hdrout = ResourceDescriptorHeap[params.hdrcolourtex];
	RWTexture2D<float4> curr = ResourceDescriptorHeap[params.currtexture];

	float4 const prevc = curr[dtid];
	float4 const prevlinc = float4(linearcolour(prevc.xyz), prevc.w);
	float4 hdrcol = hdrout[dtid];

	// simple reinhard tonemap
	float4 currc = float4(hdrcol.xyz / (1 + hdrcol.xyz), hdrcol.w);

	float4 accumcol = (params.accumcount * prevlinc + currc) / float(params.accumcount + 1);

	curr[dtid] = float4(srgbcolour(accumcol.xyz), accumcol.w);
}

float4 bilinearweights(float2 xy)
{
	return float4((1 - xy.x) * (1 - xy.y), xy.x * (1 - xy.y), (1 - xy.x) * xy.y, xy.x * xy.y);
}

float4 depthweights(float d, float4 hd, float dd, float4 pds, float pdn)
{
	float sigma = 1.0f;
	float weightcutoff = 0.03;

	float4 dw = saturate(1.0 - abs(pds) * pdn);
	dw *= dw >= weightcutoff;

	return dw;
}

float4 normalweights(float3 n, float4x3 hn)
{
	float4 nohns = float4(dot(n, hn[0]), dot(n, hn[1]), dot(n, hn[2]), dot(n, hn[3]));

	float4 nw = pow(saturate(nohns), 32.0);

	return nw;
}

float4 bilateralweights(float2 xy, float3 n, float d, float4x3 hn, float4 hd, float dd, float4 pds, float pdn)
{
	float4 bw = bilinearweights(xy);
	float4 dw = depthweights(d, hd, dd, pds, pdn);
	float4 nw = normalweights(n, hn);

	return bw * dw * nw;
}

bool inbounds(int2 p, uint2 dims)
{
	return all(p > 0) && all(p < dims);
}

void accum(uint2 dtid, rt::accumparams params)
{
	StructuredBuffer<rt::sceneglobals> scenebuf = ResourceDescriptorHeap[params.sceneglobals];
	StructuredBuffer<rt::viewglobals> viewbuf = ResourceDescriptorHeap[params.viewglobals];

	uint histidx = scenebuf[0].frameidx & 1u;
	uint curridx = histidx ^ 1u;

	RWTexture2D<float4> currnormaldepthtex = ResourceDescriptorHeap[params.normaldepthtex[curridx]];
	RWTexture2D<float4> prevnormaldepthtex = ResourceDescriptorHeap[params.normaldepthtex[histidx]];
	RWTexture2D<float4> currdiffradiancetex = ResourceDescriptorHeap[params.diffradiance[curridx]];
	RWTexture2D<float4> currspecradiancetex = ResourceDescriptorHeap[params.specradiance[curridx]];
	RWTexture2D<float4> prevdiffradiancetex = ResourceDescriptorHeap[params.diffradiance[histidx]];
	RWTexture2D<float4> prevspecradiancetex = ResourceDescriptorHeap[params.specradiance[histidx]];
	RWTexture2D<float4> prevhitpostex = ResourceDescriptorHeap[params.hitposition[histidx]];
	RWTexture2D<float4> currhitpostex = ResourceDescriptorHeap[params.hitposition[curridx]];

	// todo : check if these are correct
	//RWTexture2D<float4> currmomentstex = ResourceDescriptorHeap[params.momentstex[curridx]];
	RWTexture2D<float4> momentstex = ResourceDescriptorHeap[params.momentstex[0]];

	RWTexture2D<float4> histhistorylentex = ResourceDescriptorHeap[params.historylentex[histidx]];
	RWTexture2D<float4> currhistorylentex = ResourceDescriptorHeap[params.historylentex[curridx]];

	rt::viewglobals prevview = viewbuf[0];
	rt::viewglobals currview = viewbuf[1];

	float2 pixelCenter = float2(dtid) + 0.5;
	float2 motion = currhistorylentex[dtid].zw * params.dims;

	float2 histtexel = pixelCenter + motion;

	float historylen = 0.0;

	float3 prevdiff = 0.xxx;
	float3 prevspec = 0.xxx;
	float4 prevmoments = 0.xxxx;
	float currd = currnormaldepthtex[dtid].w;
	float3 currn = currnormaldepthtex[dtid].xyz;
	float3 currhitpos = currhitpostex[dtid].xyz;
	float ddxy = currhistorylentex[dtid].y;

	float planedistnorm = ddxy + 1.0 / (1.0 + currd);

	float sumw = 0.0f;
	int2 tl = int2(floor(histtexel - 0.5));

	float2 xy = frac(histtexel - (float2(tl) + 0.5));

	int4x2 s = int4x2(int2(0, 0) + tl, int2(1, 0) + tl, int2(0, 1) + tl, int2(1, 1) + tl);
	bool4 withintex = bool4(inbounds(s[0], params.dims), inbounds(s[1], params.dims), inbounds(s[2], params.dims), inbounds(s[3], params.dims));
	float4x3 hn = float4x3(prevnormaldepthtex[s[0]].xyz, prevnormaldepthtex[s[1]].xyz, prevnormaldepthtex[s[2]].xyz, prevnormaldepthtex[s[3]].xyz);
	float4 hd = float4(prevnormaldepthtex[s[0]].w, prevnormaldepthtex[s[1]].w, prevnormaldepthtex[s[2]].w, prevnormaldepthtex[s[3]].w);
	float4x3 hhitpos = float4x3(prevhitpostex[s[0]].xyz, prevhitpostex[s[1]].xyz, prevhitpostex[s[2]].xyz, prevhitpostex[s[3]].xyz);
	float4 planedists = float4(planedist(hhitpos[0], currhitpos, currn), planedist(hhitpos[1], currhitpos, currn), planedist(hhitpos[2], currhitpos, currn), planedist(hhitpos[3], currhitpos, currn));

	float4 weights = withintex * bilateralweights(xy, currn, currd, hn, hd, ddxy, planedists, planedistnorm);

	sumw = dot(1, weights);

	if (sumw > 1e-4f)
	{
		weights = weights / sumw;

		float4 shistlen = float4(histhistorylentex[s[0]].x, histhistorylentex[s[1]].x, histhistorylentex[s[2]].x, histhistorylentex[s[3]].x);

		float4x3 hdiffrad = float4x3(prevdiffradiancetex[s[0]].xyz, prevdiffradiancetex[s[1]].xyz, prevdiffradiancetex[s[2]].xyz, prevdiffradiancetex[s[3]].xyz);
		float4x3 hspecrad = float4x3(prevspecradiancetex[s[0]].xyz, prevspecradiancetex[s[1]].xyz, prevspecradiancetex[s[2]].xyz, prevspecradiancetex[s[3]].xyz);
		float4x4 hmoms = float4x4(momentstex[s[0]], momentstex[s[1]], momentstex[s[2]], momentstex[s[3]]);

		prevdiff = hdiffrad[0] * weights[0] + hdiffrad[1] * weights[1] + hdiffrad[2] * weights[2] + hdiffrad[3] * weights[3];
		prevspec = hspecrad[0] * weights[0] + hspecrad[1] * weights[1] + hspecrad[2] * weights[2] + hspecrad[3] * weights[3];
		prevmoments = hmoms[0] * weights[0] + hmoms[1] * weights[1] + hmoms[2] * weights[2] + hmoms[3] * weights[3];

		historylen = dot(weights, shistlen);
		historylen = round(historylen);
	}

	historylen = min(historylen + 1.0, 32.0);

	// params.alpharad and params.alphamom unused
	float radalpha = rcp(historylen);
	float momalpha = radalpha;

	float difflum = luminance(currdiffradiancetex[dtid].xyz);
	float speclum = luminance(currspecradiancetex[dtid].xyz);

	float4 diffspecmoms = float4(difflum, difflum * difflum, speclum, speclum * speclum);
	diffspecmoms = lerp(prevmoments, diffspecmoms, momalpha);

	float4 diffradvar, specradvar;
	diffradvar.w = max(0.0, diffspecmoms[1] - diffspecmoms[0] * diffspecmoms[0]);
	specradvar.w = max(0.0, diffspecmoms[3] - diffspecmoms[2] * diffspecmoms[2]);

	diffradvar.xyz = lerp(prevdiff, currdiffradiancetex[dtid].xyz, radalpha);
	specradvar.xyz = lerp(prevspec, currspecradiancetex[dtid].xyz, radalpha);

	currdiffradiancetex[dtid] = diffradvar;
	currspecradiancetex[dtid] = specradvar;
	momentstex[dtid] = diffspecmoms;
	currhistorylentex[dtid].x = historylen;
}

[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
	StructuredBuffer<rt::accumparams> paramsbuffer = ResourceDescriptorHeap[descriptorsidx.dispatchparams];
	rt::accumparams params = paramsbuffer[0];
	StructuredBuffer<rt::ptsettings> ptsettingsbuffer = ResourceDescriptorHeap[params.ptsettings];

	if (ptsettingsbuffer[0].simpleaccum)
		simpleaccum(dtid, params);
	else
		accum(dtid, params);
}
