#include "lighting.hlsli"
#include "common.hlsli"
#include "shared/common.h"

//StructuredBuffer<instance_data> instances : register(t1);

float4 main(meshshadervertex input) : SV_TARGET
{
    StructuredBuffer<gfx::objdescriptors> descriptors = ResourceDescriptorHeap[descriptorsidx.value];
    StructuredBuffer<object_constants> objconstants = ResourceDescriptorHeap[descriptors[0].objconstants];

    float4 const ambientcolor = 0.5 * objconstants[0].mat.diffuse;
    //float4 const color = computelighting(globals.lights, instances[input.instanceid].mat, input.position, normalize(input.normal));

    float4 finalcolor = float4(1, 0, 0, 1);
    finalcolor.a = objconstants[0].mat.diffuse.a;

    return finalcolor;
}