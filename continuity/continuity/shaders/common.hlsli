#pragma once
#include "shared/sharedconstants.h"

// globals
// object constants
// various dispatch params(numprims, etc)
// vertrices
// instance data
// ps texture
// ps sampler

// todo : no constant bufffers anymore. this singature should be deleted as it is outdated
#define ROOT_SIG "CBV(b0), \
                  CBV(b1), \
                  RootConstants(b2, num32bitconstants=7), \
                  SRV(t0), \
                  SRV(t1), \
                  DescriptorTable(SRV(t2), visibility = SHADER_VISIBILITY_PIXEL ), \
                  StaticSampler(s0, visibility = SHADER_VISIBILITY_PIXEL)"

struct msdata
{
    uint start;
    uint numprims;
};

struct payloaddata
{
    msdata data[MAX_MSGROUPS_PER_ASGROUP];
};

struct dispatch_parameters
{
    uint numprims;
    uint numverts_perprim;
    uint maxprims_permsgroup;
    uint numprims_perinstance;
};

struct vertexin
{
    float3 position;
    float3 normal;
    float2 texcoord;
};

struct meshshadervertex
{
    uint instanceid : instance_id;
    float4 positionh : SV_Position;
    float3 position : POSITION0;
    float3 normal : NORMAL0;
};

struct texturessvertex
{
    float4 positionh : SV_Position;
    float2 texcoord : TEXCOORD0;
};

struct light
{
    float3 color;
    float range;
    float3 position;
    float3 direction;
};

struct material
{
    float4 colour;
    float roughness;
    float reflectance;
    uint metallic;
};

struct viewconstants
{
    float3 campos;
    float4x4 viewproj;
};

struct sceneglobals
{
    uint matbuffer;
    uint viewdirshading;
};

struct instance_data
{
    float4x4 matx;
    float4x4 normalmatx;
    float4x4 mvpmatx;
    uint mat;
};

// todo : replace with instance data
struct object_constants
{
    float4x4 matx;
    float4x4 normalmatx;
    float4x4 mvpmatx;
    uint mat;
};

struct rootconstants
{
    uint objdescriptors;
    uint viewglobals;
    uint sceneglobals;
};

ConstantBuffer<rootconstants> descriptorsidx : register(b0);