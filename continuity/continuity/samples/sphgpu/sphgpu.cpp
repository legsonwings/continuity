module;

#include "simplemath/simplemath.h"
#include <d3d12.h>
#include "thirdparty/d3dx12.h"

module sphgpu;

import stdx;
import vec;
import std.core;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::sphgpu>(view_data const& data)
{
	return std::move(std::make_unique<sphgpu>(data));
}

}

using namespace DirectX;

// params
static constexpr uint numparticles = 49;
static constexpr float roomextents = 1.6f;
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

gfx::resourcelist sphgpu::create_resources()
{
    using geometry::cube;
    using geometry::sphere;
    using gfx::bodyparams;
    using gfx::material;

    auto& globalres = gfx::globalresources::get();
    auto& globals = globalres.cbuffer().data();

    // initialize lights
    globals.numdirlights = 1;
    globals.numpointlights = 2;

    globals.ambient = { 0.1f, 0.1f, 0.1f, 1.0f };
    globals.lights[0].direction = vector3{ 0.3f, -0.27f, 0.57735f }.Normalized();
    globals.lights[0].color = { 0.2f, 0.2f, 0.2f };

    globals.lights[1].position = { -15.f, 15.f, -15.f };
    globals.lights[1].color = { 1.f, 1.f, 1.f };
    globals.lights[1].range = 40.f;

    globals.lights[2].position = { 15.f, 15.f, -15.f };
    globals.lights[2].color = { 1.f, 1.f, 1.f };
    globals.lights[2].range = 40.f;

    globals.viewproj = (globalres.get().view().view * globalres.get().view().proj).Transpose();
    globalres.cbuffer().updateresource();

    globalres.addpso("sphgpu_render_particles", L"", L"sph_render_particles_ms.cso", L"default_ps.cso");
    globalres.addcomputepso("sphgpuinit", L"sphgpuinit_cs.cso");
    globalres.addcomputepso("sphgpudensitypressure", L"sphgpu_densitypressure_cs.cso");
    globalres.addcomputepso("sphgpuposition", L"sphgpu_position_cs.cso");

    // since these use static vertex buffers, just send 0 as maxverts
    boxes.emplace_back(cube{ vector3{0.f, 0.f, 0.f}, vector3{ roomextents } }, &cube::vertices_flipped, &cube::instancedata, bodyparams{ 0, 1, "instanced" });

    gfx::resourcelist res;

    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes)) { stdx::append(b->create_resources(), res); };

    // todo : is there a way to know exactly size of particle data in cpu
    static constexpr uint particle_datasize = 72;
    databuffer.createresources(numparticles * particle_datasize);

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

        auto cmd_list = globalres.cmdlist();

        auto const& pipelineobjects = globalres.psomap().find("sphgpuinit")->second;

        cmd_list->SetPipelineState(pipelineobjects.pso.Get());
        cmd_list->SetComputeRootSignature(pipelineobjects.root_signature.Get());
        cmd_list->SetComputeRootConstantBufferView(0, globalres.cbuffer().gpuaddress());
        cmd_list->SetComputeRootUnorderedAccessView(1, databuffer.gpuaddress());
        cmd_list->SetComputeRoot32BitConstants(2, 11, &rootconstants, 0);

        uint const dispatchx = stdx::ceil(float(numparticles) / 64.0f);
        cmd_list->Dispatch(dispatchx, 1, 1);

        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(databuffer._buffer.Get());
        cmd_list->ResourceBarrier(1, &uavBarrier);
    }

    return res;
}

void sphgpu::update(float dt)
{
    sample_base::update(dt);
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes)) b->update(dt);
}

void sphgpu::render(float dt)
{
    auto& globalres = gfx::globalresources::get();
    auto& globals = globalres.cbuffer().data();

    globals.viewproj = (globalres.get().view().view * globalres.get().view().proj).Transpose();
    globalres.cbuffer().updateresource();

    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes)) b->render(dt, { false });

    struct
    {
        uint32_t numparticles;
        float dt;
        float particleradius;
        float h;
        float containerorigin[3];
        float hsqr;
        float containerextents[3];
        float k;
        float rho0;
        float viscosityconstant;
        float poly6coeff;
        float poly6gradcoeff;
        float spikycoeff;
        float viscositylapcoeff;
    } rootconstants;

    rootconstants.numparticles = numparticles;
    rootconstants.containerorigin[0] = rootconstants.containerorigin[1] = rootconstants.containerorigin[2] = 0.0f;
    rootconstants.containerextents[0] = rootconstants.containerextents[1] = rootconstants.containerextents[2] = roomextents;
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

    uint const dispatchx = stdx::ceil(float(numparticles) / 64.0f);

    auto cmd_list = globalres.cmdlist();

    // todo : simulation is unstable at dt, use smaller time step
    // todo : use timestep from courant condition, but this might be tricky in gpu implementation
    // sim passes
    {
        auto const& pipelineobjects = globalres.psomap().find("sphgpudensitypressure")->second;

        // root signature is same for sim shaders
        cmd_list->SetComputeRootSignature(pipelineobjects.root_signature.Get());
        cmd_list->SetComputeRootConstantBufferView(0, globalres.cbuffer().gpuaddress());
        cmd_list->SetComputeRootUnorderedAccessView(1, databuffer.gpuaddress());
        cmd_list->SetComputeRoot32BitConstants(2, 18, &rootconstants, 0);

        // todo : handle time step 

        // simulation passes
        // density and pressure
        {
            cmd_list->SetPipelineState(pipelineobjects.pso.Get());
            cmd_list->Dispatch(dispatchx, 1, 1);

            auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(databuffer._buffer.Get());
            cmd_list->ResourceBarrier(1, &uavBarrier);
        }

        // acceleration, velocity and position
        {
            auto const& pipelineobjects = globalres.psomap().find("sphgpuposition")->second;

            cmd_list->SetPipelineState(pipelineobjects.pso.Get());
            cmd_list->Dispatch(dispatchx, 1, 1);

            auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(databuffer._buffer.Get());
            cmd_list->ResourceBarrier(1, &uavBarrier);
        }
    }

    // rendering
    {
        auto const& pipelineobjects = globalres.psomap().find("sphgpu_render_particles")->second;

        cmd_list->SetGraphicsRootSignature(pipelineobjects.root_signature.Get());
        cmd_list->SetGraphicsRootConstantBufferView(0, globalres.cbuffer().gpuaddress());
        cmd_list->SetPipelineState(pipelineobjects.pso.Get());
        cmd_list->SetGraphicsRootUnorderedAccessView(1, databuffer.gpuaddress());
        cmd_list->SetGraphicsRoot32BitConstants(2, 18, &rootconstants, 0);

        uint const dispatchx = stdx::ceil(float(numparticles) / 128.0f);
        stdx::cassert(dispatchx < 65536u, "Dispatch dimentsion limit reached");
        cmd_list->DispatchMesh(dispatchx, 1, 1);
    }
}
