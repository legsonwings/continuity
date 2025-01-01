//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

// struct Viewport
// {
//     float left;
//     float top;
//     float right;
//     float bottom;
// };

// struct RayGenConstantBuffer
// {
//     Viewport viewport;
//     Viewport stencil;
// };

// RaytracingAccelerationStructure Scene : register(t0, space0);
// RWTexture2D<float4> RenderTarget : register(u0);
// ConstantBuffer<RayGenConstantBuffer> g_rayGenCB : register(b0);

struct sceneconstants
{
    float3 campos;
    uint padding0;
    float4 ambient;
    light lights[MAX_NUM_LIGHTS];
    float4x4 viewproj;
    uint numdirlights;
    uint numpointlights;
};

// Scene wide resources.
//  g_* - bound via a global root signature.
//  l_* - bound via a local root signature.
RaytracingAccelerationStructure g_scene : register(t0, space0);
RWTexture2D<float4> g_renderTarget : register(u0);
ConstantBuffer<sceneconstants> g_sceneCB : register(b0);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};

bool IsInsideViewport(float2 p, Viewport viewport)
{
    return (p.x >= viewport.left && p.x <= viewport.right)
        && (p.y >= viewport.top && p.y <= viewport.bottom);
}

[shader("raygeneration")]
void MyRaygenShader()
{
    float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

    // Orthographic projection since we're raytracing in screen space.
    float3 rayDir = float3(0, 0, 1);
    float3 origin = float3(lerp(g_rayGenCB.viewport.left, g_rayGenCB.viewport.right, lerpValues.x), lerp(g_rayGenCB.viewport.top, g_rayGenCB.viewport.bottom, lerpValues.y), 0.0f);

    if (IsInsideViewport(origin.xy, g_rayGenCB.stencil))
    {
        // Trace the ray.
        // Set the ray's extents.
        RayDesc ray;
        ray.Origin = origin;
        ray.Direction = rayDir;
        // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
        // TMin should be kept small to prevent missing geometry at close contact areas.
        ray.TMin = 0.001;
        ray.TMax = 10000.0;
        RayPayload payload = { float4(0, 0, 0, 0) };
        TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

        // Write the raytraced color to the output texture.
        RenderTarget[DispatchRaysIndex().xy] = payload.color;
    }
    else
    {
        // Render interpolated DispatchRaysIndex outside the stencil window
        RenderTarget[DispatchRaysIndex().xy] = float4(lerpValues, 0, 1);
    }
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = float4(0, 0, 0, 1);
}

// Generate a ray in world space for a camera pixel corresponding to an index from the dispatched 2D grid.
inline Ray GenerateCameraRay(uint2 index, in float3 cameraPosition, in float4x4 projectionToWorld)
{
    float2 xy = index + 0.5f; // center in the middle of the pixel.
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates.
    screenPos.y = -screenPos.y;

    // Unproject the pixel coordinate into a world positon.
    float4 world = mul(float4(screenPos, 0, 1), projectionToWorld);
    world.xyz /= world.w;

    Ray ray;
    ray.origin = cameraPosition;
    ray.direction = normalize(world.xyz - ray.origin);

    return ray;
}

[shader("raygeneration")]
void MyRaygenShader()
{
    // Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
    Ray ray = GenerateCameraRay(DispatchRaysIndex().xy, g_sceneCB.campos, g_sceneCB.projectionToWorld);

    // Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    // Set TMin to a zero value to avoid aliasing artifacts along contact areas.
    // Note: make sure to enable face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    RayPayload rayPayload = { float4(0, 0, 0, 0)};

    TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, rayDesc, rayPayload);

    // Write the raytraced color to the output texture.
    g_renderTarget[DispatchRaysIndex().xy] = rayPayload.color;
}

//***************************************************************************
//******************------ Closest hit shaders -------***********************
//***************************************************************************

[shader("closesthit")]
void MyClosestHitShader_Triangle(inout RayPayload payload)
{
    payload.color = float4(1.0, 0.0, 1.0, 1);
}

//[shader("closesthit")]
//void MyClosestHitShader_AABB(inout RayPayload payload)
//{
//    payload.color = float4(0.0, 1.0, 1.0, 1);
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
    if (x0 > x1) swap(x0, x1);

    return true;
}

// Calculate a normal for a hit point on a sphere.
float3 CalculateNormalForARaySphereHit(in Ray ray, in float thit, float3 center)
{
    float3 hitPosition = ray.origin + thit * ray.direction;
    return normalize(hitPosition - center);
}

// Analytic solution of an unbounded ray sphere intersection points.
// Ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool SolveRaySphereIntersectionEquation(in Ray ray, inout float tmin, inout float tmax, in float3 center, in float radius)
{
    float3 L = ray.origin - center;
    float a = dot(ray.direction, ray.direction);
    float b = 2 * dot(ray.direction, L);
    float c = dot(L, L) - radius * radius;
    return SolveQuadraticEqn(a, b, c, tmin, tmax);
}

// Test if a hit is valid based on specified RayFlags and <RayTMin, RayTCurrent> range.
bool IsAValidHit(in Ray ray, in float thit, in float3 hitSurfaceNormal)
{
    return IsInRange(thit, RayTMin(), RayTCurrent()) && !IsCulled(ray, hitSurfaceNormal);
}

// Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects a hollow sphere.
bool RaySphereIntersectionTest(in Ray ray, inout float thit, inout float tmax, in float3 center = float3(0, 0, 0), in float radius = 1)
{
    float t0, t1; // solutions for t if the ray intersects 

    if (!SolveRaySphereIntersectionEquation(ray, t0, t1, center, radius)) return false;
    tmax = t1;

    if (t0 < RayTMin())
    {
        // t0 is before RayTMin, let's use t1 instead .
        if (t1 < RayTMin()) return false; // both t0 and t1 are before RayTMin

        float3 normal = CalculateNormalForARaySphereHit(ray, t1, center);
        if (IsAValidHit(ray, t1, attr.normal))
        {
            thit = t1;
            return true;
        }
    }
    else
    {
        float3 = CalculateNormalForARaySphereHit(ray, t0, center);
        if (IsAValidHit(ray, t0, attr.normal))
        {
            thit = t0;
            return true;
        }

        float3 = CalculateNormalForARaySphereHit(ray, t1, center);
        if (IsAValidHit(ray, t1, attr.normal))
        {
            thit = t1;
            return true;
        }
    }
    return false;
}

// Test if a ray with RayFlags and segment <RayTMin(), RayTCurrent()> intersects multiple hollow spheres.
bool RaySpheresIntersectionTest(in Ray ray, out float thit)
{
    const int N = 3;
    float3 centers[N] =
    {
        float3(-0.3, -0.3, -0.3),
        float3(0.1, 0.1, 0.4),
        float3(0.35,0.35, 0.0)
    };
    float  radii[N] = { 0.6, 0.3, 0.15 };
    bool hitFound = false;

    //
    // Test for intersection against all spheres and take the closest hit.
    //
    thit = RayTCurrent();

    // test against all spheres
    for (int i = 0; i < N; i++)
    {
        float _thit;
        float _tmax;
        ProceduralPrimitiveAttributes _attr = (ProceduralPrimitiveAttributes)0;
        if (RaySphereIntersectionTest(ray, _thit, _tmax, _attr, centers[i], radii[i]))
        {
            if (_thit < thit)
            {
                thit = _thit;
                attr = _attr;
                hitFound = true;
            }
        }
    }
    return hitFound;
}

[shader("intersection")]
void MyIntersectionShader_AnalyticPrimitive()
{
    Ray ray;
    ray.origin = float4(ObjectRayOrigin(), 1);
    ray.direction = ObjectRayDirection();

    float thit;
    if (RayAnalyticGeometryIntersectionTest(localRay, primitiveType, thit, attr))
    {
        PrimitiveInstancePerFrameBuffer aabbAttribute = g_AABBPrimitiveAttributes[l_aabbCB.instanceIndex];
        attr.normal = mul(attr.normal, (float3x3)aabbAttribute.localSpaceToBottomLevelAS);
        attr.normal = normalize(mul((float3x3)ObjectToWorld3x4(), attr.normal));

        ReportHit(thit, /*hitKind*/ 0, attr);
    }
}


#endif // RAYTRACING_HLSL