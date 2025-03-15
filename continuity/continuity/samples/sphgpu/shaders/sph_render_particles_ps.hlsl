#include "shaders/lighting.hlsli"

// todo : probably should remove this
// todo : alternatively use root signature that is common with default_ps adn ensure sample specific root signatures are supersets of base root signature
#include "shaders/common.hlsli"

float4 main(meshshadervertex input) : SV_TARGET
{
    // todo : use correct lighting computations
    float3 const color = float3(1.0f, 0.0f, 0.0f);
    
    float3 finalcolor = color;
    
    // todo : alpha should come from material
    float const alpha = 0.5f;

    return float4(finalcolor, alpha);
}