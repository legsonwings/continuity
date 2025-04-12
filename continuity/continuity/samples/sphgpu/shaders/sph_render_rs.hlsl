#ifndef RAYTRACE_HLSL
#define RAYTRACE_HLSL

#include "shared/raytracecommon.h"
#include "shaders/lighting.hlsli"
#include "sphcommon.hlsli"

struct raypayload
{
    float4 color;
};

struct ray
{
    float3 origin;
    float3 direction;
};

[shader("miss")]
void missshader(inout raypayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}

// generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline ray generateray(uint2 index, in float3 campos, in float4x4 projtoworld)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // unproject the pixel coordinate into a world positon.
    float4 world = mul(projtoworld, float4(screenPos, 0, 1));
    world.xyz /= world.w;

    ray ray;
    ray.origin = campos;
    ray.direction = normalize(world.xyz - ray.origin);

    return ray;
}

[shader("raygeneration")]
void raygenshader()
{
    ConstantBuffer<rt::sceneconstants> frameconstants = ResourceDescriptorHeap[0];
    
    // generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    ray ray = generateray(DispatchRaysIndex().xy, frameconstants.campos, frameconstants.inv_viewproj);

    // set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;

    // set TMin to a zero value to avoid aliasing artifacts along contact areas.
    // Note: make sure to enable face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    raypayload raypayload = { float4(0, 0, 0, 0)};

    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[1];
    TraceRay(scene, RAY_FLAG_CULL_FRONT_FACING_TRIANGLES, ~0, 0, 1, 0, rayDesc, raypayload);

    RWTexture2D<float4> rendertarget = ResourceDescriptorHeap[2];

    // write the raytraced color to the output texture.
    rendertarget[DispatchRaysIndex().xy] = raypayload.color;
}

struct dummyattributes 
{
    uint dummymember;
};

[shader("closesthit")]
void MyClosestHitShader_AABB(inout raypayload payload, in dummyattributes dummyattr)
{
    payload.color = float4(0.0, 1.0, 0.0, 1);
}

// Solve a quadratic equation.
// Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool SolveQuadraticEqn(float a, float b, float c, inout float x0, inout float x1)
{
    float discr = b * b - 4 * a * c;
    if (discr < 0) return false;
    else if (discr == 0) x0 = x1 = -0.5 * b / a;
    else {
        float q = (b > 0) ?
            -0.5 * (b + sqrt(discr)) :
            -0.5 * (b - sqrt(discr));
        x0 = q / a;
        x1 = c / q;
    }
    if (x0 > x1)
    {
        float t = x1;
        x1 = x0;
        x0 = t;
    }
    return true;
 }

// Calculate a normal for a hit point on a sphere.
float3 CalculateNormalForARaySphereHit(in ray ray, in float thit, float3 center)
{
    float3 hitPosition = ray.origin + thit * ray.direction;
    return normalize(hitPosition - center);
}

// Analytic solution of an unbounded ray sphere intersection points.
// Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool SolveRaySphereIntersectionEquation(in ray ray, inout float tmin, inout float tmax, in float3 center, in float radius)
{
    float3 L = ray.origin - center;
    float a = dot(ray.direction, ray.direction);
    float b = 2 * dot(ray.direction, L);
    float c = dot(L, L) - radius * radius;
    return SolveQuadraticEqn(a, b, c, tmin, tmax);
}

bool IsInRange(in float val, in float min, in float max)
{
    return (val >= min && val <= max);
}

// Test if a hit is culled based on specified rayFlags.
bool IsCulled(in ray ray, in float3 hitSurfaceNormal)
{
    float rayDirectionNormalDot = dot(ray.direction, hitSurfaceNormal);

    bool isCulled = ((RayFlags() & RAY_FLAG_CULL_BACK_FACING_TRIANGLES) && (rayDirectionNormalDot > 0))
        || ((RayFlags() & RAY_FLAG_CULL_FRONT_FACING_TRIANGLES) && (rayDirectionNormalDot < 0));

    return isCulled;
}

// Test if a hit is valid based on specified RayFlags and <RayTMin, RayTCurrent> range.
bool IsAValidHit(in ray ray, in float thit, in float3 hitSurfaceNormal)
{
    return IsInRange(thit, RayTMin(), RayTCurrent()) && !IsCulled(ray, hitSurfaceNormal);
}

// Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects a hollow sphere.
bool RaySphereIntersectionTest(in ray ray, out float thit, in float3 center = float3(0, 0, 0), in float radius = 1)
{
    float t0, t1; // solutions for t if the ray intersects 
    thit = 1e-9;
    if (!SolveRaySphereIntersectionEquation(ray, t0, t1, center, radius))
        return false;

    if (t0 < RayTMin())
    {
        // t0 is before RayTMin, let's use t1 instead .
        if (t1 < RayTMin()) return false; // both t0 and t1 are before RayTMin

        float3 normal = CalculateNormalForARaySphereHit(ray, t1, center);
        if (IsAValidHit(ray, t1, normal))
        {
            thit = t1;
            return true;
        }
    }
    else
    {
        float3 normal = CalculateNormalForARaySphereHit(ray, t0, center);
        if (IsAValidHit(ray, t0, normal))
        {
            thit = t0;
            return true;
        }

        normal = CalculateNormalForARaySphereHit(ray, t1, center);
        if (IsAValidHit(ray, t1, normal))
        {
            thit = t1;
            return true;
        }
    }
    return false;
}

// Test if a ray segment <RayTMin(), RayTCurrent()> intersects an AABB.
// Limitation: this test does not take RayFlags into consideration and does not calculate a surface normal.
// Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
bool rayaabb_intersection(ray ray, float3 aabb[2], out float tmin, out float tmax)
{
    float3 tmin3, tmax3;
    int3 sign3 = ray.direction > 0;

    // Handle rays parallel to any x|y|z slabs of the AABB.
    // If a ray is within the parallel slabs, 
    //  the tmin, tmax will get set to -inf and +inf
    //  which will get ignored on tmin/tmax = max/min.
    // If a ray is outside the parallel slabs, -inf/+inf will
    //  make tmax > tmin fail (i.e. no intersection).
    // TODO: handle cases where ray origin is within a slab 
    //  that a ray direction is parallel to. In that case
    //  0 * INF => NaN
    const float FLT_INFINITY = 1.#INF;
    float3 invRayDirection = select(ray.direction != 0, 1 / ray.direction, select(ray.direction > 0, FLT_INFINITY, -FLT_INFINITY));

    tmin3.x = (aabb[1 - sign3.x].x - ray.origin.x) * invRayDirection.x;
    tmax3.x = (aabb[sign3.x].x - ray.origin.x) * invRayDirection.x;

    tmin3.y = (aabb[1 - sign3.y].y - ray.origin.y) * invRayDirection.y;
    tmax3.y = (aabb[sign3.y].y - ray.origin.y) * invRayDirection.y;
    
    tmin3.z = (aabb[1 - sign3.z].z - ray.origin.z) * invRayDirection.z;
    tmax3.z = (aabb[sign3.z].z - ray.origin.z) * invRayDirection.z;
    
    tmin = max(max(tmin3.x, tmin3.y), tmin3.z);
    tmax = min(min(tmax3.x, tmax3.y), tmax3.z);
    
    return tmax > tmin && tmax >= RayTMin() && tmin <= RayTCurrent();
}

// Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects a hollow AABB.
bool rayaabb_intersection(ray ray, float3 aabb[2], inout float thit /*inout ProceduralPrimitiveAttributes attr*/)
{
    float tmin, tmax;
    if (rayaabb_intersection(ray, aabb, tmin, tmax))
    {
        // Only consider intersections crossing the surface from the outside.
        if (tmin < RayTMin() || tmin > RayTCurrent())
            return false;

        thit = tmin;

        // Set a normal to the normal of a face the hit point lays on.
        float3 hitPosition = ray.origin + thit * ray.direction;
        float3 distanceToBounds[2] =
        {
            abs(aabb[0] - hitPosition),
            abs(aabb[1] - hitPosition)
        };
        
        float3 normal;
        const float eps = 0.0001;
        if (distanceToBounds[0].x < eps)
            normal = float3(-1, 0, 0);
        else if (distanceToBounds[0].y < eps)
            normal = float3(0, -1, 0);
        else if (distanceToBounds[0].z < eps)
            normal = float3(0, 0, -1);
        else if (distanceToBounds[1].x < eps)
            normal = float3(1, 0, 0);
        else if (distanceToBounds[1].y < eps)
            normal = float3(0, 1, 0);
        else if (distanceToBounds[1].z < eps)
            normal = float3(0, 0, 1);

        return true;// IsAValidHit(ray, thit, normal);
    }
    return false;
}

[shader("intersection")]
void MyIntersectionShader_AnalyticPrimitive()
{
    ray r;
    r.origin = ObjectRayOrigin();
    r.direction = ObjectRayDirection();
    
    //float3 const center = float3(0, 0, 0);
    //float const extents = 1.6;
    float3 aabb[2];
    aabb[0] = (-10.0).xxx;
    aabb[1] = (10.0).xxx;
    
    //ConstantBuffer<sphgpu_dispatch_params> sph_dispatch_params = ResourceDescriptorHeap[4];
    //RWStructuredBuffer<particle_data> particledata = ResourceDescriptorHeap[5];

    float thit;
    if (rayaabb_intersection(r, aabb, thit))
    {
        float colour = 0;
        //for (uint j = 0; j < sph_dispatch_params.numparticles; ++j)
        //{
        //    float3 const c = particledata[j].p - cellcenter;
        //    float const rcprho = rcp(particledata[j].rho);
        //    float const c2 = dot(c, c);

        //    float const t0 = a2 + c2;
        //    float const t1 = 2.0f;

        //    float3 const ac = c * machingcube_halfextent;

        //    colour[0] += pow(max(0.0f, sph_dispatch_params.hsqr - (t0 + t1 * (ac.x + ac.y + ac.z))), 3) * rcprho;
        //}

        dummyattributes attr;
        ReportHit(thit, 0, attr);
    }
}


#endif // RAYTRACE_HLSL