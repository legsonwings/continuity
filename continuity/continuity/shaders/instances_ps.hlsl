#include "lighting.hlsli"
#include "common.hlsli"
#include "shared/common.h"

static const float2 poissondisk[4] = { float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725), float2(-0.094184101, -0.92938870), float2(0.34495938, 0.29387760) };

float4 main(meshshadervertex input) : SV_TARGET
{
    StructuredBuffer<gfx::dispatchparams> dispatchparams = ResourceDescriptorHeap[descriptorsidx.dispatchparams];
    StructuredBuffer<instance_data> objconstants = ResourceDescriptorHeap[dispatchparams[0].objconstants];
    StructuredBuffer<viewconstants> viewglobals = ResourceDescriptorHeap[descriptorsidx.viewglobals];
    StructuredBuffer<sceneglobals> sceneglobals = ResourceDescriptorHeap[descriptorsidx.sceneglobals];

    float3 l = -sceneglobals[0].lightdir;
    float3 shadingpos = input.position;

    float3 n = normalize(input.normal);
    float3 v = normalize(viewglobals[0].viewpos - shadingpos);

    if (sceneglobals[0].viewdirshading == 1)
    {
        // only use vertex normals
        return float4(max(dot(v, n), 0).xxx, 1);
    }
    else
    {
        SamplerState sampler = SamplerDescriptorHeap[0];
        SamplerState pointsampler = SamplerDescriptorHeap[2];
        SamplerComparisonState shadowsampler = SamplerDescriptorHeap[3];

        Texture2D<float> shadowmap = ResourceDescriptorHeap[sceneglobals[0].shadowmap];
        float3 ndccoords = input.positionl.xyz / input.positionl.w;
        
        // xy is in range [-1,1] and z in [0, 1]
        float2 shadowmapcoords = float2(ndccoords.x * 0.5f + 0.5f, (1.0f - ndccoords.y) * 0.5f);
        float currdepth = ndccoords.z;

        static float const depthbiasbase = 0.05f;

        float shadowscale = 0.0f;
        
        // this means the fragment lies before light frustum's far plane
        if (currdepth >= 0.0f)
        {
            float depthbias = dot(n, l) * depthbiasbase;

            // each fetch does bilinear pcf
            [unroll] for(int i = -1; i <= 1; ++i)
                [unroll] for (int j = -1; j <= 1; ++j)
                    shadowscale += shadowmap.SampleCmpLevelZero(shadowsampler, shadowmapcoords, currdepth + depthbias, int2(i, j));

            shadowscale /= 9.0f;
        }

        StructuredBuffer<material> materials = ResourceDescriptorHeap[sceneglobals[0].matbuffer];

        material m = materials[input.material];

        Texture2D<float4> difftex = ResourceDescriptorHeap[m.diffusetex];
        Texture2D<float4> roughnesstex = ResourceDescriptorHeap[m.roughnesstex];
        Texture2D<float4> normaltex = ResourceDescriptorHeap[m.normaltex];

        // todo : should really be orthonormalizing
        float3 t, b;
        t = normalize(input.tangent);
        b = normalize(input.bitangent);

        float3 normal = normaltex.Sample(sampler, input.texcoords).xyz * 2.0 - 1.0;

        n = t * normal.x + b * normal.y + n * normal.z;

        float4 sampledcolour = difftex.Sample(sampler, input.texcoords);
        float3 const ambientcolor = 0.1f * sampledcolour.xyz;
        float2 mr = roughnesstex.Sample(sampler, input.texcoords).bg;

        float3 colour = calculatelighting(sceneglobals[0].lightluminance.xxx, l, v, n, sampledcolour.xyz, mr.y, m.reflectance, mr.x);
        float4 finalcolor = float4(colour * (1.0f - shadowscale) + ambientcolor, sampledcolour.a);

        return finalcolor;
    }
}