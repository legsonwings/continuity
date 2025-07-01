#include "lighting.hlsli"
#include "common.hlsli"

float4 main(meshshadervertex input) : SV_TARGET
{
    float4 const ambientcolor = float4(0.4, 0.4, 0 , 1);//globals.ambient * objectconstants.mat.diffuse;
   // float4 const color = computelighting(globals.lights, objectconstants.mat, input.position, normalize(input.normal));

    float4 finalcolor = ambientcolor + float4(1, 1, 1, 1);
    finalcolor.a = 1.0f;

    return finalcolor;
}