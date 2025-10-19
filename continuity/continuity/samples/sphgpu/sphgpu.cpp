module;

#include "simplemath/simplemath.h"
#include <d3d12.h>
#include "thirdparty/d3dx12.h"
#include "thirdparty/dxhelpers.h"

#include "shared/raytracecommon.h"

module sphgpu;

import stdx;
import vec;
import std;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::sphgpu>(view_data const& data)
{
	return std::move(std::make_unique<sphgpu>(data));
}

}

#define namkaran(d3dobject) { d3dobject->SetName(utils::strtowstr(#d3dobject).c_str()); }
#define namkaranres(gfxresource) { gfxresource.d3dresource->SetName(utils::strtowstr(#gfxresource).c_str()); }

using namespace DirectX;

// params
static constexpr uint numparticles = 10;
static constexpr float roomextents = 2.0f;
static constexpr float particleradius = 0.1f;
static constexpr float h = 0.2f; // smoothing kernel constant
static constexpr float hsqr = h * h;
static constexpr float k = 200.0f;  // pressure constant
static constexpr float rho0 = 1.0f; // reference density
static constexpr float viscosityconstant = 1.4f;
static constexpr float fixedtimestep = 0.04f;
static constexpr float surfthreshold = 0.000001f;
static constexpr float surfthresholdsqr = surfthreshold * surfthreshold;
static constexpr float isolevel = 0.000001f;
static constexpr float isolevelsqr = isolevel * isolevel;
static constexpr float maxacc = 100.0;
static constexpr float maxspeed = 20.0f;
static constexpr float sqrt2 = 1.41421356237f;
static constexpr float marchingcube_size = 0.1f;

// raytracing stuff
static constexpr char const* trihitgroupname = "trianglehitgroup";
static constexpr char const* prochitgroupname = "hitgroup_sph";
static constexpr char const* raygenshadername = "raygenshader";
static constexpr char const* triclosesthit = "tri_closesthit";
static constexpr char const* procclosesthitshadername = "sph_closesthit";
static constexpr char const* missshadername = "missshader";
static constexpr char const* intersectionshadername = "sph_intersection";

struct Viewport
{
    float left;
    float top;
    float right;
    float bottom;
};

struct RayGenConstantBuffer
{
    Viewport viewport;
    Viewport stencil;
};

constexpr float poly6kernelcoeff()
{
    return 315.0f / (64.0f * XM_PI * stdx::pown(h, 9u));
}

constexpr float poly6gradcoeff()
{
    return -1890.0f / (64.0f * XM_PI * stdx::pown(h, 9u));
}

constexpr float spikykernelcoeff()
{
    return -45.0f / (XM_PI * stdx::pown(h, 6u));
}

constexpr float viscositylaplaciancoeff()
{
    return 45.0f / (XM_PI * stdx::pown(h, 6u));
}

sphgpu::sphgpu(view_data const& viewdata) : sample_base(viewdata)
{
	camera.Init({ 0.f, 0.f, -30.f });
	camera.SetMoveSpeed(10.0f);
}

float sphgpu::computetimestep() const
{
    return 0.0f;
    //static constexpr float mintimestep = 1.0f / 240.0f;
    //static constexpr float counrant_safetyconst = 1.0f;

    //float maxcsqr = 0.0f;
    //float maxvsqr = 0.0f;
    //float maxasqr = 0.0f;

    //for (auto const& p : particleparams)
    //{
    //    float const c = speedofsoundsqr(p.rho, p.pr);
    //    maxcsqr = std::max(c * c, maxcsqr);
    //    maxvsqr = std::max(p.v.Dot(p.v), maxvsqr);
    //    maxasqr = std::max(p.a.Dot(p.a), maxasqr);
    //}

    //float maxv = std::max(0.000001f, std::sqrt(maxvsqr));
    //float maxa = std::max(0.000001f, std::sqrt(maxasqr));
    //float maxc = std::max(0.000001f, std::sqrt(maxcsqr));

    //return std::max(mintimestep, std::min({ counrant_safetyconst * h / maxv, std::sqrt(h / maxa), counrant_safetyconst * h / maxc }));
}

UINT dispatchsize(uint dimension_dispatch, uint dimension_threads_pergroup)
{
    return static_cast<UINT>((dimension_dispatch + dimension_threads_pergroup - 1) / dimension_threads_pergroup);
}

gfx::resourcelist sphgpu::create_resources(gfx::deviceresources& deviceres)
{
    using geometry::cube;
    using gfx::bodyparams;

    auto& globalres = gfx::globalresources::get();
    //constantbuffer.createresource();
    //sphconstants.createresource();

    globalres.addpso("sphgpu_render_debugparticles", "", "sph_render_debugparticles_ms.cso", "sph_render_particles_ps.cso");
    globalres.addcomputepso("sphgpuinit", "sphgpuinit_cs.cso");
    globalres.addcomputepso("sphgpudensitypressure", "sphgpu_densitypressure_cs.cso");
    globalres.addcomputepso("sphgpuposition", "sphgpu_position_cs.cso");

    gfx::resourcelist res;
    databuffer.create(numparticles);

    // dispatch initialization compute shader
    {
        struct
        {
            uint32_t numparticles;
            float dt;
            float particleradius;
            float h;
            float containerorigin[3];
            float hsqr;
            float containerextents[3];
        } rootconstants;
        
        rootconstants.numparticles = numparticles;
        rootconstants.containerorigin[0] = rootconstants.containerorigin[1] = rootconstants.containerorigin[2] = 0.0f;
        rootconstants.containerextents[0] = rootconstants.containerextents[1] = rootconstants.containerextents[2] = roomextents;

        auto const& pipelineobjects = globalres.psomap().find("sphgpuinit")->second;
        auto& cmdlist = *deviceres.cmdlist.Get();
        cmdlist.SetPipelineState(pipelineobjects.pso.Get());
        cmdlist.SetComputeRootSignature(pipelineobjects.root_signature.Get());
        
        // todo cbuffer removed
        //cmdlist.SetComputeRootConstantBufferView(0, globalres.cbuffer().currframe_gpuaddress());
        cmdlist.SetComputeRootUnorderedAccessView(1, databuffer.gpuaddress());
        cmdlist.SetComputeRoot32BitConstants(5, 11, &rootconstants, 0);

        UINT const dispatchx = dispatchsize(numparticles, 64);
        cmdlist.Dispatch(dispatchx, 1, 1);

        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(databuffer.d3dresource.Get());
        cmdlist.ResourceBarrier(1, &uavBarrier);
    }

    {
        gfx::raytraceshaders rtshaders;
        rtshaders.raygen = raygenshadername;
        rtshaders.miss = missshadername;
        rtshaders.procedural_hitgroup.name = prochitgroupname;
        rtshaders.procedural_hitgroup.closesthit = procclosesthitshadername;
        rtshaders.procedural_hitgroup.intersection = intersectionshadername;
        rtshaders.tri_hitgrp.name = trihitgroupname;
        rtshaders.tri_hitgrp.closesthit = triclosesthit;

        auto& raytracepipeline_objs = globalres.addraytracingpso("sph_render", "sph_render_rs.cso", rtshaders);

        auto& device = globalres.device();
        auto& cmdlist = globalres.cmdlist();

        stdx::vec3 roomtextents = stdx::vec3::filled(2.0f);
        geometry::aabb aabb(-roomtextents, roomtextents);

        gfx::blasinstancedescs instancedescs;

        // one material id per blas
        std::vector<uint32> material_idsdata(2u);
        std::vector<gfx::material> materials_data(1u);

        material_idsdata[0] = 0;
        material_idsdata[1] = 0;

        materials_data[0].basecolour = { 1.0f, 0.0f, 0.0f, 1.0f };
        materials_data[0].metallic = 1u;
        materials_data[0].roughness = 0.25f;
        materials_data[0].reflectance = 0.5f;

        gfx::model model("models/cornellbox.obj", res);

        //res.push_back(roomvertbuffer.create(model.vertices));
        //res.push_back(roomindexbuffer.create(model.indices));
        //res.push_back(materialids.create(material_idsdata));
        //res.push_back(materials.create(materials_data));

        // todo : vertices now has normals too
        //roomvertbuffer.create(model.vertices);
        //roomindexbuffer.create(model.indices);
        //materialids.create(material_idsdata);
        //materials.create(materials_data);

        for (auto r : triblas.build(instancedescs, gfx::geometryopacity::opaque, roomvertbuffer, roomindexbuffer))
            res.push_back(r);

        for (auto r : procblas.build(instancedescs, gfx::geometryopacity::transparent, aabb))
            res.push_back(r);

        for (auto r : tlas.build(instancedescs))
            res.push_back(r);

        stdx::cassert(material_idsdata.size() == instancedescs.descs.size(), "all geometry instances should have material ids specified");

        ComPtr<ID3D12StateObjectProperties> stateobjectproperties;
        ThrowIfFailed(raytracepipeline_objs.pso_raytracing.As(&stateobjectproperties));
        auto* stateobjectprops = stateobjectproperties.Get();

        // get shader id's
        void* raygenshaderid = stateobjectprops->GetShaderIdentifier(utils::strtowstr(raygenshadername).c_str());
        void* missshaderid = stateobjectprops->GetShaderIdentifier(utils::strtowstr(missshadername).c_str());
        void* hitgroupshaderids_aabbgeometry = stateobjectprops->GetShaderIdentifier(utils::strtowstr(prochitgroupname).c_str());
        void* hitgroupshaderids_trianglegeometry = stateobjectprops->GetShaderIdentifier(utils::strtowstr(trihitgroupname).c_str());

        // now build shader tables
        // only one records for ray gen and miss shader tables
        raygenshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
        raygenshadertable.addrecord(raygenshaderid);

        missshadertable = gfx::shadertable(gfx::shadertable_recordsize<void>::size, 1);
        missshadertable.addrecord(missshaderid);

        // procedural aabb records for hitgroup shader table
        hitgroupshadertable = gfx::shadertable(gfx::shadertable_recordsize<void, void>::size, 2);
        hitgroupshadertable.addrecord(hitgroupshaderids_trianglegeometry);
        hitgroupshadertable.addrecord(hitgroupshaderids_aabbgeometry);

        raytracingoutput.create(DXGI_FORMAT_R8G8B8A8_UNORM, stdx::vecui2{ viewdata.width, viewdata.height }, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    
        // we don't store descriptors, as the indices are hardcoded in shaders
        // 0 rt srv
        // 1 rt uav
        // 2 const buffer 
        // 3 sph const buffer
        // 4 tlas
        // 5 ray trace output
        // 6 room vertex buffer
        // 7 room index buffer
        // 8 material ids
        // 9 material data
        // 10 particle data buffer
        
        // todo : bindless isn't hardcorded anymore
        //constantbuffer.createcbv();
        //sphconstants.createcbv();
        //tlas.createsrv();
        //raytracingoutput.createuav();
        //roomvertbuffer.createsrv();
        //roomindexbuffer.createsrv();
        //materialids.createsrv();
        //materials.createsrv();
        //databuffer.createsrv();
    }

    return res;
}

void sphgpu::update(float dt)
{
    sample_base::update(dt);
}

void sphgpu::render(float dt, gfx::renderer& renderer)
{
    struct
    {
        uint32_t numparticles;
        float dt;
        float particleradius;
        float h;
        stdx::vec3 containerorigin; // todo : is anything going to need this?
        float hsqr;
        stdx::vec3 containerextents;
        float k;
        stdx::vec3 marchingcubeoffset;
        float rho0;
        float viscosityconstant;
        float poly6coeff;
        float poly6gradcoeff;
        float spikycoeff;
        float viscositylapcoeff;
        float isolevel;
        float marchingcubesize;
    } rootconstants;

    rootconstants.numparticles = numparticles;
    rootconstants.containerorigin.fill(0.0f);
    rootconstants.containerextents.fill(roomextents);
    rootconstants.dt = dt;
    rootconstants.particleradius = particleradius;
    rootconstants.h = h;
    rootconstants.hsqr = hsqr;
    rootconstants.k = k;
    rootconstants.rho0 = rho0;
    rootconstants.viscosityconstant = viscosityconstant;
    rootconstants.poly6coeff = poly6kernelcoeff();
    rootconstants.poly6gradcoeff = poly6gradcoeff();
    rootconstants.spikycoeff = spikykernelcoeff();
    rootconstants.viscositylapcoeff = viscositylaplaciancoeff();
    rootconstants.isolevel = isolevel;
    rootconstants.marchingcubesize = marchingcube_size;

    UINT const simdispatchx = dispatchsize(numparticles, 64);

    auto& globalres = gfx::globalresources::get();
    auto& cmdlist = *renderer.deviceres().cmdlist.Get();

    // todo : simulation is unstable at dt, use smaller time step
    // todo : use timestep from courant condition, but this might be tricky in gpu implementation
    // sim passes
    {
        auto const& pipelineobjects = globalres.psomap().find("sphgpudensitypressure")->second;

        // root signature is same for sim shaders
        cmdlist.SetComputeRootSignature(pipelineobjects.root_signature.Get());
        cmdlist.SetComputeRootUnorderedAccessView(1, databuffer.gpuaddress());
        cmdlist.SetComputeRoot32BitConstants(5, 23, &rootconstants, 0);

        // todo : handle time step 
        // simulation passes
        // density and pressure
        {
            cmdlist.SetPipelineState(pipelineobjects.pso.Get());
            gfx::uav_barrier(cmdlist, databuffer);

            cmdlist.Dispatch(simdispatchx, 1, 1);
        }

        // acceleration, velocity and position
        {
            auto const& pipelineobjects = globalres.psomap().find("sphgpuposition")->second;

            cmdlist.SetPipelineState(pipelineobjects.pso.Get());

            gfx::uav_barrier(cmdlist, databuffer);

            cmdlist.Dispatch(simdispatchx, 1, 1);

            gfx::uav_barrier(cmdlist, databuffer);
        }
    }

    // todo : pass these to ResourceDescriptorHeap
    //sphconstants sphparams;

    //sphparams.numparticles = numparticles;
    //sphparams.containerextents.fill(5);
    //sphparams.dt = dt;
    //sphparams.particleradius = particleradius;
    //sphparams.h = h;
    //sphparams.hsqr = hsqr;
    //sphparams.k = k;
    //sphparams.rho0 = rho0;
    //sphparams.viscosityconstant = viscosityconstant;
    //sphparams.poly6coeff = poly6kernelcoeff();
    //sphparams.poly6gradcoeff = poly6gradcoeff();
    //sphparams.spikycoeff = spikykernelcoeff();
    //sphparams.viscositylapcoeff = viscositylaplaciancoeff();
    //sphparams.isolevel = isolevel;

    //auto& framecbuffer = constantbuffer.data(0);

    //framecbuffer.sundir = stdx::vec3::filled(1.0f).normalized();
    //framecbuffer.campos = camera.GetCurrentPosition();
    //framecbuffer.inv_viewproj = utils::to_matrix4x4((globalres.view().view * globalres.view().proj).Invert());

    auto const& pipelineobjects = globalres.psomap().find("sph_render")->second;

    // bind the global root signature, heaps and frame index, other resources are bindless
    cmdlist.SetDescriptorHeaps(1, globalres.resourceheap().d3dheap.GetAddressOf());
    cmdlist.SetComputeRootSignature(pipelineobjects.root_signature.Get());
    cmdlist.SetPipelineState1(pipelineobjects.pso_raytracing.Get());

    gfx::raytrace rt;
    rt.dispatchrays(raygenshadertable, missshadertable, hitgroupshadertable, pipelineobjects.pso_raytracing.Get(), viewdata.width, viewdata.height);
    rt.copyoutputtorendertarget(&cmdlist, raytracingoutput, renderer.rendertarget->d3dresource.Get());
}