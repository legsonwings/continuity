#include "common.hlsli"
#include "shaders/brdf.hlsli"

float attenuation(float range, float distance)
{
	return 1.f / (1.f + (distance * distance) / (range * range));
}

float3 specularcoefficient(float3 r0, float3 normal, float3 lightvec)
{
	float const oneminuscos = 1.f - saturate(dot(normal, lightvec));
	return r0 + (1 - r0) * pow(oneminuscos, 5.f);
}

float3 blinnphong(float3 toeye, float3 normal, float3 intensity, material mat, float3 lightvec)
{
	float const m = (1.f - mat.roughness) * 256.f;
	float3 const h = normalize((lightvec + toeye));

	float3 const specular = specularcoefficient(mat.reflectance.xxx, h, lightvec);
	float3 const roughness =  (m + 8.f ) * pow(max(dot(h, normal), 0.f), m) / 8.f;
	float3 specalbedo = specular * roughness;
	
	// specalbedo can go out of range [0 1] which LDR doesn't support
	specalbedo = specalbedo / (specalbedo + 1.0f);

	return (mat.colour.rgb + specalbedo) * intensity;
}

float3 directionallight(light l, material m, float3 normal, float3 toeye)
{
	float3 const lightvec = -l.direction;
	float3 const lightintensity = l.color * max(dot(lightvec, normal), 0.f);
	return blinnphong(toeye, normal, lightintensity, m, lightvec);
}

float3 pointlight(light l, material m, float3 normal, float3 pos, float3 toeye)
{
	float3 lightvec = l.position - pos;
	float const d = length(lightvec);
	lightvec /= d;

	float3 const lightintensity = l.color * saturate(dot(lightvec, normal)) * attenuation(l.range, d);

	return blinnphong(toeye, normal, lightintensity, m, lightvec);
}

float4 computelighting(light lights[MAX_NUM_LIGHTS], material m, float3 pos, float3 normal)
{
	float3 result = 0;
	//float3 const toeye = normalize(globals.campos - pos);
	//
	//int i = 0;
	//for (; i < globals.numdirlights; ++i)
	//{
	//	result += directionallight(lights[i], m, normal, toeye);
	//}

	//for (; i < globals.numdirlights + globals.numpointlights; ++i)
	//{
	//	result += pointlight(lights[i], m, normal, pos, toeye);
	//}

	return float4(result, 1.f);
}

// irradiance is amount of light energy a surface recieves per unit area, at normal incidence
// assume all surfaces recieve same amount of light energy at normal incidence for now,
// this assumption holds well for directional lights which are assumed to be infinitely far away, but for point and spot lights we would need to attenuate the irradiance
float3 calculatelighting(float3 irradiance, float3 l, float3 v, float3 n, float3 c, float r, float fr, float m)
{
    bool ismetal = m > 0.95f;
    // metals do not have diffuse reflectance and non-metals have low specular reflectance
    float3 diffusealbedo = ismetal ? (float3) 0.0f : c;
    float3 sepcularalbedo = ismetal ? c : (float3) 0.04f; // constant low specular reflectance for dielectrics

    // dielectric specular reflectance is f0 = 0.16f * reflectance2
    // metallic specular reflectance is base colour
    // see Lagarde's "Moving Frostbite to PBR"
    float3 f0dielectric = (0.16f * fr * fr).xxx;
    float3 f0metallic = c;

    float3 f0 = lerp(f0metallic, f0dielectric, m);

    // map roughness from perceptually linear roughness
    r = r * r;
    float3 specularcolour = specularbrdf(l, v, n, r, f0);
    float brdfd = diffusebrdf();
    
    float3 diffusecolour = brdfd * diffusealbedo;
    float3 reflectedcolour = diffusecolour + specularcolour;

    float nol = saturate(dot(n, l));
    return irradiance * nol * reflectedcolour;
}
