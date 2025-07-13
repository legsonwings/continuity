#include "lighting.hlsli"
#include "common.hlsli"
#include "shared/common.h"

float4 main(meshshadervertex input) : SV_TARGET
{
    StructuredBuffer<gfx::objdescriptors> descriptors = ResourceDescriptorHeap[descriptorsidx.objdescriptors];
    StructuredBuffer<instance_data> objconstants = ResourceDescriptorHeap[descriptors[0].objconstants];
    StructuredBuffer<viewconstants> viewgloabls = ResourceDescriptorHeap[descriptorsidx.viewglobals];
    StructuredBuffer<sceneglobals> sceneglobals = ResourceDescriptorHeap[descriptorsidx.sceneglobals];

    StructuredBuffer<material> materials = ResourceDescriptorHeap[sceneglobals[0].matbuffer];
    SamplerState linearsampler = SamplerDescriptorHeap[0];

    material m = materials[input.material];
    Texture2D<float4> difftex = ResourceDescriptorHeap[m.diffusetex];

    // todo : sundir should come from outside
    float3 l = -float3(1, -1, 1);
    float3 shadingpos = input.position;

    float3 n = normalize(input.normal);
    float3 v = normalize(viewgloabls[0].campos - shadingpos);

    if (sceneglobals[0].viewdirshading == 1)
    {
        return float4(max(dot(v, n), 0).xxx, 1);
    }
    else
    {
        float4 sampledcolour = difftex.Sample(linearsampler, input.texcoords);
        float3 const ambientcolor = 0.1f * sampledcolour.xyz;

        float3 colour = calculatelighting(float3(20, 20, 20), l, v, n, sampledcolour.xyz, m.roughness, m.reflectance, m.metallic);
        float4 finalcolor = float4(colour + ambientcolor, sampledcolour.a);

        return finalcolor;
    }
}