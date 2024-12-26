module;

#include "simplemath/simplemath.h"
#include <d3d12.h>
#include "thirdparty/d3dx12.h"
#include "thirdparty/dxhelpers.h"

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

#define namkaran(d3dobject) { d3dobject->SetName(utils::strtowstr(#d3dobject).c_str()); }
#define namkaranres(gfxresource) { gfxresource.d3dresource->SetName(utils::strtowstr(#gfxresource).c_str()); }

using namespace DirectX;

// params
static constexpr uint numparticles = 2;
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
static constexpr float marchingcube_size = 0.1f;

// raytracing stuff
constexpr char const* hitGroupName = "MyHitGroup";
constexpr char const* raygenShaderName = "MyRaygenShader";
constexpr char const* closestHitShaderName = "MyClosestHitShader";
constexpr char const* missShaderName = "MyMissShader";

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

UINT dispatchsize(uint total_workitems, uint threads_pergroup)
{
    return static_cast<UINT>((total_workitems + threads_pergroup - 1) / threads_pergroup);
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

    globalres.addpso("sphgpu_render_particles", "", "sph_render_particles_ms.cso", "sph_render_particles_ps.cso", gfx::psoflags::transparent | gfx::psoflags::twosided);
    globalres.addpso("sphgpu_render_debugparticles", "", "sph_render_debugparticles_ms.cso", "sph_render_particles_ps.cso");
    globalres.addcomputepso("sphgpuinit", "sphgpuinit_cs.cso");
    globalres.addcomputepso("sphgpudensitypressure", "sphgpu_densitypressure_cs.cso");
    globalres.addcomputepso("sphgpuposition", "sphgpu_position_cs.cso");
    globalres.addcomputepso("marchingcubes", "marchingcubes_cs.cso");

    // todo : add engine support for indirect dispatch
    // command signature used for indirect drawing.
    {
        D3D12_INDIRECT_ARGUMENT_DESC argumentDesc;
        argumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;

        D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
        commandSignatureDesc.pArgumentDescs = &argumentDesc;
        commandSignatureDesc.NumArgumentDescs = 1u;
        commandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);

        ThrowIfFailed(globalres.device()->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&render_commandsig)));
        NAME_D3D12_OBJECT(render_commandsig);
    }

    // since these use static vertex buffers, just send 0 as maxverts
    boxes.emplace_back(cube{ vector3{0.f, 0.f, 0.f}, vector3{ roomextents } }, &cube::vertices_flipped, &cube::instancedata, bodyparams{ 0, 1, "instanced" });

    gfx::resourcelist res;
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes)) { stdx::append(b->create_resources(), res); };

    //// todo : is there a way to know exactly size of particle data in cpu
    //// todo : there is, use common header for cpp and hlsl struct, take care of padding
    //static constexpr uint particle_datasize = 72;

    //databuffer.createresources("particledata", numparticles * particle_datasize);

    //// todo : creating the counter before vertices buffer causes a crash
    //// what the fuck

    //// each marching cube can write upto 5 triangles(15 float3's)
    //// this can store 66,660 vertices
    //isosurface_vertices.createresources("isosurface_vertices", 1000000);
    //isosurface_vertices_counter.createresources("isosurface_vertices_counter", sizeof(uint32_t));
    //render_args.createresources("render_dispatch_args", sizeof(D3D12_DISPATCH_ARGUMENTS));
    //namkaranres(isosurface_vertices);
    //namkaranres(isosurface_vertices_counter);
    //namkaranres(render_args);

    //// dispatch initialization compute shader
    //{
    //    struct
    //    {
    //        uint32_t numparticles;
    //        float dt;
    //        float particleradius;
    //        float h;
    //        float containerorigin[3];
    //        float hsqr;
    //        float containerextents[3];
    //    } rootconstants;
    //    
    //    rootconstants.numparticles = numparticles;
    //    rootconstants.containerorigin[0] = rootconstants.containerorigin[1] = rootconstants.containerorigin[2] = 0.0f;
    //    rootconstants.containerextents[0] = rootconstants.containerextents[1] = rootconstants.containerextents[2] = roomextents;

    //    auto cmd_list = globalres.cmdlist();

    //    auto const& pipelineobjects = globalres.psomap().find("sphgpuinit")->second;

    //    cmd_list->SetPipelineState(pipelineobjects.pso.Get());
    //    cmd_list->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    //    cmd_list->SetComputeRootConstantBufferView(0, globalres.cbuffer().gpuaddress());
    //    cmd_list->SetComputeRootUnorderedAccessView(1, databuffer.gpuaddress());
    //    cmd_list->SetComputeRoot32BitConstants(5, 11, &rootconstants, 0);

    //    UINT const dispatchx = dispatchsize(numparticles, 64);
    //    cmd_list->Dispatch(dispatchx, 1, 1);

    //    auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(databuffer.d3dresource.Get());
    //    cmd_list->ResourceBarrier(1, &uavBarrier);
    //}

    // rt triangle
    {
        gfx::raytraceshaders rtshaders;
        rtshaders.raygen = raygenShaderName;
        rtshaders.closesthit = closestHitShaderName;
        rtshaders.miss = missShaderName;

        auto& raytraceipelinepipeline_objs = globalres.addraytracingpso("hellotriangle", "hellotriangle_rs.cso", hitGroupName, rtshaders);

        auto& device = globalres.device();
        auto& cmdlist = globalres.cmdlist();

        struct vertexonly { float v0, v1, v2; };
        unsigned short indices[] = { 0, 1, 2 };

        float depthValue = 1.0;
        float offset = 0.7f;
        vertexonly vertices[] =
        {
            // The sample raytraces in screen space coordinates.
            // Since DirectX screen space coordinates are right handed (i.e. Y axis points down).
            // Define the vertices in counter clockwise order ~ clockwise in left handed.
            { 0, -offset, depthValue },
            { -offset, offset, depthValue },
            { offset, offset, depthValue }
        };

        auto vertexbuffer = gfx::create_uploadbufferwithdata(&vertices, sizeof(vertices));
        auto indexbuffer = gfx::create_uploadbufferwithdata(&indices, sizeof(indices));
        namkaran(vertexbuffer);
        namkaran(indexbuffer);

        // todo : vertex and index buffer descriptors to be passed using descriptor heaps, so craete srvs

        res.push_back(vertexbuffer);
        res.push_back(indexbuffer);

        // now build acceleration structures
        // the sample resets the command allocator before acceleration structure build, but we dont, do we need it?

        D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
        geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geometryDesc.Triangles.IndexBuffer = indexbuffer->GetGPUVirtualAddress();
        geometryDesc.Triangles.IndexCount = static_cast<UINT>(indexbuffer->GetDesc().Width) / sizeof(unsigned short);
        geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
        geometryDesc.Triangles.Transform3x4 = 0;
        geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geometryDesc.Triangles.VertexCount = static_cast<UINT>(vertexbuffer->GetDesc().Width) / sizeof(vertexonly);
        geometryDesc.Triangles.VertexBuffer.StartAddress = vertexbuffer->GetGPUVirtualAddress();
        geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(vertexonly);

        // Mark the geometry as opaque. 
        // PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
        // Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
        geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        // Get required sizes for an acceleration structure.
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
        topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        topLevelInputs.Flags = buildFlags;
        topLevelInputs.NumDescs = 1;
        topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
        device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
        stdx::cassert(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = topLevelInputs;
        bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        bottomLevelInputs.pGeometryDescs = &geometryDesc;
        device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
        stdx::cassert(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

        ComPtr<ID3D12Resource> scratchResource = gfx::create_default_uavbuffer(std::max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes));
        namkaran(scratchResource);
        res.push_back(scratchResource);

        // Allocate resources for acceleration structures.
        // Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
        // Default heap is OK since the application doesn’t need CPU read/write access to them. 
        // The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
        // and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
        //  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
        //  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
        {
            toplevelaccelerationstructure = gfx::create_accelerationstructbuffer(topLevelPrebuildInfo.ResultDataMaxSizeInBytes);
            bottomlevelaccelerationstructure = gfx::create_accelerationstructbuffer(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes);
            namkaran(toplevelaccelerationstructure);
            namkaran(bottomlevelaccelerationstructure);
        }

        // Create an instance desc for the bottom-level acceleration structure.
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
        instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
        instanceDesc.InstanceMask = 1;
        instanceDesc.AccelerationStructure = bottomlevelaccelerationstructure->GetGPUVirtualAddress();

        ComPtr<ID3D12Resource> instanceDescs = gfx::create_uploadbufferwithdata(&instanceDesc, sizeof(instanceDesc));
        namkaran(instanceDescs);
        res.push_back(instanceDescs);

        // Bottom Level Acceleration Structure desc
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
        {
            bottomLevelBuildDesc.Inputs = bottomLevelInputs;
            bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
            bottomLevelBuildDesc.DestAccelerationStructureData = bottomlevelaccelerationstructure->GetGPUVirtualAddress();
        }

        // Top Level Acceleration Structure desc
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
        {
            topLevelInputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
            topLevelBuildDesc.Inputs = topLevelInputs;
            topLevelBuildDesc.DestAccelerationStructureData = toplevelaccelerationstructure->GetGPUVirtualAddress();
            topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
        }

        auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
            {
                auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(bottomlevelaccelerationstructure.Get());
                raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
                raytracingCommandList->ResourceBarrier(1, &barrier);
                raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
            };

        // Build acceleration structure.
        BuildAccelerationStructure(cmdlist.Get());

        // now build shader tables

        void* rayGenShaderIdentifier;
        void* missShaderIdentifier;
        void* hitGroupShaderIdentifier;

        auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
        {
            rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(utils::strtowstr(raygenShaderName).c_str());
            missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(utils::strtowstr(missShaderName).c_str());
            hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(utils::strtowstr(hitGroupName).c_str());
        };

        ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
        ThrowIfFailed(raytraceipelinepipeline_objs.pso_raytracing.As(&stateObjectProperties));
        GetShaderIdentifiers(stateObjectProperties.Get());

        // Ray gen shader table
        {
            struct RootArguments 
            {
                RayGenConstantBuffer cb;
            };

            uint const datasize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(RootArguments);

            // todo : move this to stdx.core
            uint const aligneduploadsize = (datasize + (D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1)) & ~(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1);
            
            std::vector<std::byte> uploaddata;
            uploaddata.reserve(aligneduploadsize);

            // todo : doing two mem copies, better to do one directly into mapped address
            memcpy(uploaddata.data(), rayGenShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            RootArguments& rootarguments = reinterpret_cast<RootArguments&>(*(uploaddata.data() + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));
            rootarguments.cb.viewport = { -1.0f, -1.0f, 1.0f, 1.0f };
            float border = 0.1f;
            rootarguments.cb.stencil = { -1 + border, -1 + border, 1.0f - border, 1 - border };
            rayGenShaderTable = gfx::create_uploadbufferwithdata(uploaddata.data(), aligneduploadsize);
            namkaran(rayGenShaderTable);
        }

        constexpr uint shaderidentifier_aligneduploadsize = (D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + (D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1)) & ~(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1);
        std::byte uploaddata[shaderidentifier_aligneduploadsize];

        // Miss shader table
        {
            // todo : doing two mem copies, better to do one directly into mapped address
            memcpy(uploaddata, missShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            missShaderTable = gfx::create_uploadbufferwithdata(uploaddata, shaderidentifier_aligneduploadsize);
            namkaran(missShaderTable);
        }

        // Hit group shader table
        {
            // todo : doing two mem copies, better to do one directly into mapped address
            memcpy(uploaddata, hitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            hitGroupShaderTable = gfx::create_uploadbufferwithdata(uploaddata, shaderidentifier_aligneduploadsize);
            namkaran(hitGroupShaderTable);
        }

        // Create the output resource. The dimensions and format should match the swap-chain.
        auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 720, 720, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&raytracingOutput)));
        namkaran(raytracingOutput);

        auto descriptorHeapCpuBase = globalres.srvheap()->GetCPUDescriptorHandleForHeapStart();

        D3D12_CPU_DESCRIPTOR_HANDLE cpudescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, 0, UINT(gfx::srvcbvuav_descincrementsize()));
        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(raytracingOutput.Get(), nullptr, &UAVDesc, cpudescriptor);
        raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(globalres.srvheap()->GetGPUDescriptorHandleForHeapStart(), 0, UINT(gfx::srvcbvuav_descincrementsize()));
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
    //auto& globals = globalres.cbuffer().data();

    //globals.viewproj = (globalres.get().view().view * globalres.get().view().proj).Transpose();
    //globalres.cbuffer().updateresource();

    //// render the room inner walls
    //for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes)) b->render(dt, { false });
    //
    //// we need two additional marches in each dimensions so that we don't miss isosurface really close to the wall due to precision issues
    //// because of particles near the walls we can have isosurfaces outside the bounds
    //// in this case just two marches is not enough in each dimension as for large 'h' the additional cubes might never intersect surface of a sphere of radius 'h'
    //static constexpr bool collect_isosurfaces_outside_bounds = true;
    //uint const additional_marches_perdim = (collect_isosurfaces_outside_bounds ? stdx::ceil(h / marchingcube_size) : 1u) * 2u;
    //uint const marches_perdim = stdx::ceil(roomextents / marchingcube_size) + additional_marches_perdim;

    //struct
    //{
    //    uint32_t numparticles;
    //    float dt;
    //    float particleradius;
    //    float h;
    //    stdx::vec3 containerorigin; // todo : is anything going to need this?
    //    float hsqr;
    //    stdx::vec3 containerextents;
    //    float k;
    //    stdx::vec3 marchingcubeoffset;
    //    float rho0;
    //    float viscosityconstant;
    //    float poly6coeff;
    //    float poly6gradcoeff;
    //    float spikycoeff;
    //    float viscositylapcoeff;
    //    float isolevel;
    //    float marchingcubesize;
    //} rootconstants;

    //constexpr auto halfextents = roomextents / 2.0f;

    //rootconstants.numparticles = numparticles;
    //rootconstants.containerorigin = stdx::vec3::fill(0.0f);
    //rootconstants.containerextents = stdx::vec3::fill(roomextents);
    //rootconstants.marchingcubeoffset = rootconstants.containerorigin - stdx::vec3::fill(halfextents + (additional_marches_perdim / 2u) * marchingcube_size);
    //rootconstants.dt = dt;
    //rootconstants.particleradius = particleradius;
    //rootconstants.h = h;
    //rootconstants.hsqr = hsqr;
    //rootconstants.k = k;
    //rootconstants.rho0 = rho0;
    //rootconstants.viscosityconstant = viscosityconstant;
    //rootconstants.poly6coeff = poly6kernelcoeff();
    //rootconstants.poly6gradcoeff = poly6gradcoeff();
    //rootconstants.spikycoeff = spikykernelcoeff();
    //rootconstants.viscositylapcoeff = viscositylaplaciancoeff();
    //rootconstants.isolevel = isolevel;
    //rootconstants.marchingcubesize = marchingcube_size;

    //UINT const simdispatchx = dispatchsize(numparticles, 64);
    //UINT const marchingcubesdispatch = UINT(marches_perdim);

    //auto cmd_list = globalres.cmdlist();

    //// todo : simulation is unstable at dt, use smaller time step
    //// todo : use timestep from courant condition, but this might be tricky in gpu implementation
    //// sim passes
    //{
    //    auto const& pipelineobjects = globalres.psomap().find("sphgpudensitypressure")->second;

    //    // root signature is same for sim shaders
    //    cmd_list->SetComputeRootSignature(pipelineobjects.root_signature.Get());
    //    cmd_list->SetComputeRootConstantBufferView(0, globalres.cbuffer().gpuaddress());
    //    cmd_list->SetComputeRootUnorderedAccessView(1, databuffer.gpuaddress());
    //    cmd_list->SetComputeRoot32BitConstants(5, 23, &rootconstants, 0);

    //    // todo : handle time step 
    //    // simulation passes
    //    // density and pressure
    //    {
    //        cmd_list->SetPipelineState(pipelineobjects.pso.Get());
    //        cmd_list->SetComputeRootUnorderedAccessView(3, isosurface_vertices_counter.gpuaddress());
    //        cmd_list->SetComputeRootUnorderedAccessView(2, render_args.gpuaddress());
    //        
    //        gfx::uav_barrier(cmd_list, databuffer, isosurface_vertices_counter,render_args);

    //        cmd_list->Dispatch(simdispatchx, 1, 1);
    //    }

    //    // acceleration, velocity and position
    //    {
    //        auto const& pipelineobjects = globalres.psomap().find("sphgpuposition")->second;

    //        cmd_list->SetPipelineState(pipelineobjects.pso.Get());

    //        gfx::uav_barrier(cmd_list, databuffer);

    //        cmd_list->Dispatch(simdispatchx, 1, 1);
    //    }

    //    // polygonization
    //    {
    //        auto const& pipelineobjects = globalres.psomap().find("marchingcubes")->second;

    //        cmd_list->SetPipelineState(pipelineobjects.pso.Get());
    //        cmd_list->SetComputeRootUnorderedAccessView(3, isosurface_vertices_counter.gpuaddress());
    //        cmd_list->SetComputeRootUnorderedAccessView(2, render_args.gpuaddress());
    //        cmd_list->SetComputeRootUnorderedAccessView(4, isosurface_vertices.gpuaddress());

    //        gfx::uav_barrier(cmd_list, databuffer, isosurface_vertices, isosurface_vertices_counter, render_args);

    //        cmd_list->Dispatch(marchingcubesdispatch, marchingcubesdispatch, marchingcubesdispatch);
    //    }
    //}

    //// rendering
    //{
    //    bool debugp = true;
    //    if (debugp)
    //    {
    //        auto const& pipelineobjects = globalres.psomap().find("sphgpu_render_debugparticles")->second;

    //        cmd_list->SetGraphicsRootSignature(pipelineobjects.root_signature.Get());
    //        cmd_list->SetPipelineState(pipelineobjects.pso.Get());
    //        cmd_list->SetGraphicsRootConstantBufferView(0, globalres.cbuffer().gpuaddress());
    //        cmd_list->SetGraphicsRootUnorderedAccessView(1, databuffer.gpuaddress());
    //        cmd_list->SetGraphicsRoot32BitConstants(5, 23, &rootconstants, 0);

    //        UINT const dispatchdebugx = dispatchsize(numparticles, 85);
    //        stdx::cassert(dispatchdebugx < 65536u, "Dispatch dimentsion limit reached");
    //        cmd_list->DispatchMesh(dispatchdebugx, 1, 1);
    //    }

    //    {
    //        auto const& pipelineobjects = globalres.psomap().find("sphgpu_render_particles")->second;

    //        cmd_list->SetGraphicsRootSignature(pipelineobjects.root_signature.Get());
    //        cmd_list->SetPipelineState(pipelineobjects.pso.Get());
    //        cmd_list->SetGraphicsRootConstantBufferView(0, globalres.cbuffer().gpuaddress());
    //        cmd_list->SetGraphicsRootUnorderedAccessView(1, databuffer.gpuaddress());
    //        cmd_list->SetGraphicsRootUnorderedAccessView(3, isosurface_vertices_counter.gpuaddress());
    //        cmd_list->SetGraphicsRootUnorderedAccessView(4, isosurface_vertices.gpuaddress());
    //        cmd_list->SetGraphicsRoot32BitConstants(5, 23, &rootconstants, 0);

    //        gfx::uav_barrier(cmd_list, isosurface_vertices, isosurface_vertices_counter, render_args);

    //        cmd_list->ExecuteIndirect(render_commandsig.Get(), 1u, render_args.d3dresource.Get(), 0u, nullptr, 0u);
    //    }
    //}
    // 
    // 
    
    auto cmd_list = globalres.cmdlist();

    auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
    {
        // Since each shader table has only one shader record, the stride is same as the size.
        dispatchDesc->HitGroupTable.StartAddress = hitGroupShaderTable->GetGPUVirtualAddress();
        dispatchDesc->HitGroupTable.SizeInBytes = hitGroupShaderTable->GetDesc().Width;
        dispatchDesc->HitGroupTable.StrideInBytes = dispatchDesc->HitGroupTable.SizeInBytes;
        dispatchDesc->MissShaderTable.StartAddress = missShaderTable->GetGPUVirtualAddress();
        dispatchDesc->MissShaderTable.SizeInBytes = missShaderTable->GetDesc().Width;
        dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
        dispatchDesc->RayGenerationShaderRecord.StartAddress = rayGenShaderTable->GetGPUVirtualAddress();
        dispatchDesc->RayGenerationShaderRecord.SizeInBytes = rayGenShaderTable->GetDesc().Width;
        dispatchDesc->Width = 720;
        dispatchDesc->Height = 720;
        dispatchDesc->Depth = 1;
        commandList->SetPipelineState1(stateObject);
        commandList->DispatchRays(dispatchDesc);
    };

    auto const& pipelineobjects = globalres.psomap().find("hellotriangle")->second;
    cmd_list->SetComputeRootSignature(pipelineobjects.root_signature.Get());

    // Bind the heaps, acceleration structure and dispatch rays.    
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    cmd_list->SetDescriptorHeaps(1, globalres.srvheap().GetAddressOf());
    cmd_list->SetComputeRootDescriptorTable(0, raytracingOutputResourceUAVGpuDescriptor);
    cmd_list->SetComputeRootShaderResourceView(1, toplevelaccelerationstructure->GetGPUVirtualAddress());
    DispatchRays(cmd_list.Get(), pipelineobjects.pso_raytracing.Get(), &dispatchDesc);

    auto* rendertarget = globalres.rendertarget().Get();

    D3D12_RESOURCE_BARRIER preCopyBarriers[2];
    preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(rendertarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
    preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    cmd_list->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

    cmd_list->CopyResource(rendertarget, raytracingOutput.Get());

    D3D12_RESOURCE_BARRIER postCopyBarriers[2];
    postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(rendertarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    cmd_list->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}