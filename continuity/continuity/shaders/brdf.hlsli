#include "shared/raytracecommon.h"

#ifndef BRDF_HLSL
#define BRDF_HLSL

float ggx_g1(float r2, float noh)
{
    float noh2 = noh * noh;
    float tantheta = max(1 - noh2, 0) / noh2;
    return 2 / (1 + sqrt(1 + r2 * tantheta));
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
// this version cancels the (4 * nol * nov) from the denominator so omit it from the brdf
float ggx_specularvisibility(float nol, float nov, float r2)
{
    // see https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/normaldistributionfunction(specularg)
    float ggt1 = nol * sqrt(nov * nov * (1.0f - r2) + r2);
    float ggt2 = nov * sqrt(nol * nol * (1.0f - r2) + r2);

    return 0.5f / max(ggt1 + ggt2, 1e-10f);
}

float3 fresnel_schlick(float loh, float3 f0, float f90)
{
    return f0 + (f90.xxx - f0) * pow(1.0f - loh, 5.0f);
}

// approximate pre-integrated specular brdf. the approximation assumes ggx vndf and schlick's approximation.
// see Eq 4 in [Ray Tracing Gems, Chapter 32]
// also see nvidia rtxpt
float3 approxspecularggxintegral(float3 specularalbedo, float alpha, float nov)
{
    // nov
    float costheta = abs(nov);

    float4 x = float4(1, costheta, costheta * costheta, costheta * costheta * costheta);
    float4 y = float4(1, alpha, alpha * alpha, alpha * alpha * alpha);

    float2x2 m1 = float2x2(0.995367f, -1.38839f,
                          -0.24751f, 1.97442f);

    float3x3 m2 = float3x3(1.0f, 2.68132f, 52.366f,
                            16.0932f, -3.98452f, 59.3013f,
                            -5.18731f, 255.259f, 2544.07f);

    float2x2 m3 = float2x2(-0.0564526f, 3.82901f,
                            16.91f, -11.0303f);

    float3x3 m4 = float3x3(1.0f, 4.11118f, -1.37886f,
                            19.3254f, -28.9947f, 16.9514f,
                            0.545386f, 96.0994f, -79.4492f);

    float bias = dot(mul(m1, x.xy), y.xy) * rcp(dot(mul(m2, x.xyw), y.xyw));
    float scale = dot(mul(m3, x.xy), y.xy) * rcp(dot(mul(m4, x.xzw), y.xyw));

    // this is a hack for specular reflectance of 0
    float specularreflectanceluma = dot(specularalbedo, float3((1.f / 3.f).xxx));
    bias *= saturate(specularreflectanceluma * 50.0f);

    return mad(specularalbedo, max(0.0, scale), max(0.0, bias));
}

float diffusebrdf()
{
    // lambertian diffuse brdf needs to be energy conservative, that is where the pi term comes in
    // with this we can have diffuse colour be specified within range [0-1] and still satisfy energy conservation constraint
    // see https://www.rorydriscoll.com/2009/01/25/energy-conservation-in-games/
    return rcp(pi);
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

struct ggxsampleeval
{
    float3 dir;

    // brdf is premultiplied by (nol / pdf)
    float3 brdf;
};

template<typename t>
ggxsampleeval eval(float a, float3 v, float2 u, float3 n, float3 f0)
{
    t ggxsampler;
    return ggxsampler.eval(a, v, u, n, f0);
}

template<typename st>
struct ggxsampler
{
    ggxsampleeval eval(float a, float3 v, float2 u, float3 n, float3 f0)
    {
        ggxsampleeval res;
        float3 b = perpendicular(n);
        float3 t = cross(b, n);

        // convert to local space v
        v = normalize(float3(dot(v, t), dot(v, b), dot(v, n)));
        float3 h = normalize(((st)this).sample(a, v, u));

        // loh and voh are same since we reflect
        float loh = dot(v, h);

        // this should be normalized if h and v are normalized
        float3 l = 2.f * loh * h - v;

        // note : saturate before reflection somehow causes minor artefacts at glancing angles
        loh = saturate(loh);

        float const nol = saturate(l.z);
        float const nov = saturate(v.z);

        float const a2 = a * a;
        float const g = ggx_specularvisibility(nov, nol, a2);
        float3 const f = fresnel_schlick(loh, f0, 1.0f);

        // notes: 
        // brdf's 4 * nov * nol terms don't show up as they cancel those from g term above
        // brdf is premultiplied with nol and pre divided by pdf
        res.brdf = (g * f) * nol / ((st)this).pdfnondf(a, v, h, loh);
        res.dir = normalize(t * l.x + b * l.y + n * l.z);

        return res;
    }
};

// see "Microfacet Models for Refraction through Rough Surfaces"(https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf)
struct ggxndf : ggxsampler<ggxndf>
{
    float3 sample(float a, float3 v, float2 u)
    {
        float a2 = a * a;
        float phi = 2 * pi * u.y;
        float tanth2 = a2 * u.x / (1 - u.x);
        float costh = 1 / sqrt(1 + tanth2);
        float r = sqrt(max(1 - costh * costh, 0));

        return float3(cos(phi) * r, sin(phi) * r, costh);
    }

    float pdfnondf(float al, float3 v, float3 m, float loh)
    {
        float const voh = loh;
        float const noh = m.z;

        // pdf : d * noh / (4 * voh)
        return noh / (4 * voh);
    }
};

// see "Sampling the GGX Distribution of Visible Normals"(https://jcgt.org/published/0007/04/01/paper.pdf)
// this gives unbiased normals that lie on the positive hemisphere
struct ggxvndf : ggxsampler<ggxvndf>
{
    // sample 
    float3 sample(float a, float3 v, float2 u)
    {
        float2 alpha = a.xx;

        // transform the view vector to the hemisphere configuration.
        float3 vh = normalize(float3(alpha.x * v.x, alpha.y * v.y, v.z));

        // construct orthonormal basis (v,t1,t2)  
        float lensq = vh.x * vh.x + vh.y * vh.y;
        float3 t1 = lensq > 0.0001999f ? float3(-vh.y, vh.x, 0) * rsqrt(lensq) : float3(1, 0, 0);
        float3 t2 = cross(vh, t1);

        // parameterization of the projected area of the hemisphere.
        float r = sqrt(u.x);
        float phi = 2.0 * pi * u.y;
        float rcos = r * cos(phi);
        float rsin = r * sin(phi);
        float s = 0.5f * (1.0 + vh.z);
        rsin = (1.0 - s) * sqrt(1.0 - rcos * rcos) + s * rsin;

        // reproject onto hemisphere.
        float3 nh = rcos * t1 + rsin * t2 + sqrt(max(0.f, 1.0 - rcos * rcos - rsin * rsin)) * vh;

        // transform the normal back to the ellipsoid configuration. This is our half vector.
       return normalize(float3(alpha.x * nh.x, alpha .y * nh.y, max(0.f, nh.z)));
    }

    float pdfnondf(float al, float3 v, float3 m, float loh)
    {
        float const nov = max(v.z, 1e-10);
        float g1 = ggx_g1(al * al, nov);

        // pdf : g1 * d * voh / (4 * nov * voh)
        // todo : the form in the paper seems different but rtxpt uses this form with "voh" in denom(?)
        // note : 4 * nov can be cancled with one in brdf if we were to specialize evaluation for vndf
        return g1 / (4 * nov);
    }
};

// based on https://gpuopen.com/download/Bounded_VNDF_Sampling_for_Smith-GGX_Reflections.pdf
struct ggxbvndf : ggxsampler<ggxbvndf>
{
    float3 sample(float al, float3 v, float2 u)
    {
        float2 alpha = al.xx;

        float3 i_std = normalize(float3(v.xy * alpha, v.z));

        // sample a spherical cap
        float phi = 2.0f * pi * u.x;
        float a = saturate(min(alpha.x, alpha.y)); // Eq. 6
        float s = 1.0f + length(float2(v.x, v.y)); // Omit sgn for a <=1
        float a2 = a * a; float s2 = s * s;
        float k = (1.0f - a2) * s2 / (s2 + a2 * v.z * v.z); // Eq. 5
        float b = v.z > 0 ? k * i_std.z : i_std.z;
        float z = mad(1.0f - u.y, 1.0f + b, -b);
        float sinTheta = sqrt(saturate(1.0f - z * z));
        float3 o_std = float3(sinTheta * cos(phi), sinTheta * sin(phi), z);
        
        // compute the microfacet normal m
        float3 m_std = i_std + o_std;
        float3 m = normalize(float3(m_std.xy * alpha, m_std.z));

        // transform the normal back to the ellipsoid configuration. This is our half vector. From this we can compute reflection vector with reflect(-ViewVector, h);
        return normalize(float3(m_std.xy * alpha, m_std.z));
    }

    float pdfnondf(float al, float3 v, float3 m, float loh)
    {
        float2 alpha = al.xx;
        float2 ai = alpha * v.xy;
        float len2 = dot(ai, ai);
        float t = sqrt(len2 + v.z * v.z);

        // no check for z >= 0 like paper does as the view vector should be in positive hemisphere
        float a = saturate(min(alpha.x, alpha.y)); // Eq. 6
        float s = 1.0f + length(float2(v.x, v.y)); // omit sgn for a <=1
        float a2 = a * a;
        float s2 = s * s;
        float k = (1.0f - a2) * s2 / (s2 + a2 * v.z * v.z); // Eq. 5
        return 1.0 / (2.0f * (k * v.z + t)); // Eq. 8 * || dm/do ||
    }
};

#endif