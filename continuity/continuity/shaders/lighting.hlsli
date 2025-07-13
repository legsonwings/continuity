#include "common.hlsli"

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

// normal distribution function
// return the amount of microfacets that would reflect light along view direction
float ggx_specularndf(float noh, float r2)
{
    // see [Walter 07]
    float t = noh * noh * (r2 - 1.0f) + 1;

    return r2 / (pi * t * t);
}

// visibility function that takes microfacet heights into account
float ggx_specularvisibility(float nol, float nov, float r2)
{
    // see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/normaldistributionfunction(speculard)
    float ggt1 = nol * sqrt(nov * nov * (1.0f - r2) + r2);
    float ggt2 = nov * sqrt(nol * nol * (1.0f - r2) + r2);

    return 0.5f / (ggt1 + ggt2);
}

float3 fresnel_schlick(float loh, float3 f0, float f90)
{
    return f0 + (f90.xxx - f0) * pow(1.0f - loh, 5.0f);
}

float diffusebrdf()
{
    // lambertian diffuse brdf needs to be energy conservative, that is where the pi term comes in
    // with this we can have diffuse colour be specified within range [0-1] and still satisfy energy conservation constraint
    // see https://www.rorydriscoll.com/2009/01/25/energy-conservation-in-games/
    return 1.0 / pi;
}

float3 specularbrdf(float3 l, float3 v, float3 n, float r, float3 f0)
{
    float3 h = normalize(l + v);

    float noh = saturate(dot(n, h));
    float nov = saturate(dot(n, v));
    float nol = saturate(dot(n, l));
    float loh = saturate(dot(h, l));

    float r2 = r * r;
    float ndf = ggx_specularndf(noh, r2);
    float vis = ggx_specularvisibility(nov, nol, r2);
    float3 f = fresnel_schlick(loh, f0, 1.0f);
    
    return ndf * vis * f;
}

// irradiance is amount of light energy a surface recieves per unit area, at normal incidence
// assume all surfaces recieve same amount of light energy at normal incidence for now,
// this assumption holds well for directional lights which are assumed to be infinitely far away, but for point and spot lights we would need to attenuate the irradiance
float3 calculatelighting(float3 irradiance, float3 l, float3 v, float3 n, float3 c, float r, float fr, uint m)
{
    bool ismetallic = m > 0.001f;
    // metals do not have diffuse reflectance and non-metals have low specular reflectance
    float3 diffusealbedo = ismetallic ? (float3) 0.0f : c;
    float3 sepcularalbedo = ismetallic ? c : (float3) 0.04f; // constant low specular reflectance for dielectrics

    // dielectric specular reflectance is f0 = 0.16f * reflectance2
    // metallic specular reflectance is base colour
    // see Lagarde's "Moving Frostbite to PBR"
    float3 f0dielectric = (0.16f * fr * fr).xxx;
    float3 f0metallic = c;

    float3 f0 = ismetallic ? f0metallic : f0dielectric;

    // map roughness from perceptually linear roughness
    r = r * r;
    float3 brdfs = specularbrdf(l, v, n, r, f0);
    float brdfd = diffusebrdf();
    
    float3 diffusecolour = brdfd * diffusealbedo;
    float3 specularcolour = brdfs * sepcularalbedo;
    float3 reflectedcolour = diffusecolour + specularcolour;

    float nol = saturate(dot(n, l));
    return irradiance * nol * reflectedcolour;
}
