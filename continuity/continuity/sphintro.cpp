module;

#include "simplemath/simplemath.h"

module sphintro;

import stdx;
import vec;
import std.core;

namespace sample_creator
{

template <>
std::unique_ptr<sample_base> create_instance<samples::sphintro>(view_data const& data)
{
	return std::move(std::make_unique<sphfluidintro>(data));
}

}

std::random_device rd;
static std::mt19937 re(rd());

using namespace DirectX;

std::vector<vector3> fillwithspheres(geometry::aabb const& box, uint count, float radius)
{
    std::unordered_set<uint> occupied;
    std::vector<vector3> spheres;

    auto const roomspan = (box.span() - stdx::tolerance<vector3>);

    stdx::cassert(roomspan.x > 0.f);
    stdx::cassert(roomspan.y > 0.f);
    stdx::cassert(roomspan.z > 0.f);

    uint const degree = static_cast<uint>(std::ceil(std::cbrt(count))) - 1;
    uint const numcells = (degree + 1) * (degree + 1) * (degree + 1);
    float const gridvol = numcells * 8 * (radius * radius * radius);

    // find the size of largest cube that can be fit into box
    float const cubelen = std::min({ roomspan.x, roomspan.y, roomspan.z });
    float const celld = cubelen / (degree + 1);
    float const cellr = celld / 2.f;;

    // check if this box can contain all cells
    stdx::cassert(cubelen * cubelen * cubelen > gridvol);

    vector3 const gridorigin = { box.center() - vector3(cubelen / 2.f) };
    for (auto i : stdx::range(0u, count))
    {
        static std::uniform_int_distribution<uint> distvoxel(0u, numcells - 1);

        uint emptycell = 0;
        while (occupied.find(emptycell) != occupied.end()) emptycell = distvoxel(re);

        occupied.insert(emptycell);

        auto const thecell = stdx::grididx<2>::from1d(degree, emptycell);
        spheres.emplace_back((vector3(thecell[0] * celld, thecell[2] * celld, thecell[1] * celld) + vector3(cellr)) + gridorigin);
    }

    return spheres;
}

// params
static constexpr uint numparticles = 900u;
static constexpr float roomextents = 8.0f;
static constexpr float particleradius = 0.1f;
static constexpr float h = 1.0f; // smoothing kernel constant
static constexpr float hsqr = h * h;
static constexpr float mass = 1.0f;   // particle mass
static constexpr float k = 20.0f;  // pressure constant
static constexpr float rho0 = 1.0f; // reference density
static constexpr float viscosityconstant = 0.5f;
static constexpr float fixedtimestep = 0.04f;
static constexpr float surfthreshold = 0.0f;
static constexpr float surfthresholdsqr = surfthreshold * surfthreshold;
static constexpr float maxacc = 1500.0f;
static constexpr float maxspeed = 20.0f;

sphfluid::sphfluid(geometry::aabb const& _bounds) : container(_bounds)
{
    particlegeometry = geometry::sphere{ {0.0f, 0.0f, 0.0f }, particleradius };
   
    auto const& redmaterial = gfx::globalresources::get().mat("ball");
    particleparams.reserve(numparticles);
    static std::uniform_real_distribution<float> distvel(-0.1f, 0.1f);
    static std::uniform_real_distribution<float> distacc(-0.1f, 0.1f);

    auto const intialhalfspan = _bounds.span() / (2.0f);
    geometry::aabb initial_bounds = { _bounds.center() - intialhalfspan, _bounds.center() + intialhalfspan };
    for (auto const& center : fillwithspheres(initial_bounds, numparticles, particleradius))
    {
        particleparams.emplace_back();
        particleparams.back().p = center;

        // todo : expose this limit of 5000 somewhere
        // better yet make it a template parameter of the generator function
        stdx::cassert(numparticles <= 5000u);
        particleparams.back().matname = gfx::generaterandom_matcolor(redmaterial);

        // give small random acceleration and velocity to particles
        particleparams.back().v = vector3(distvel(re), distvel(re), distvel(re));
        particleparams.back().a = vector3(distacc(re), distacc(re), distacc(re));

        // compute velocity at -half time step
        particleparams.back().vp = particleparams.back().v - (0.5f * fixedtimestep) * particleparams.back().a;
    }
}

float speedofsoundsqr(float density, float pressure)
{
    static constexpr float specificheatratio = 1.0f;
    return density < 0.000000001f ? 0.0f : specificheatratio * pressure / density;
}

float sphfluid::computetimestep() const
{
    static constexpr float mintimestep = 1.0f / 240.0f;
    static constexpr float counrant_safetyconst = 1.0f;

    float maxcsqr = 0.0f;
    float maxvsqr = 0.0f;
    float maxasqr = 0.0f;

    for (auto const& p : particleparams)
    {
        float const c = speedofsoundsqr(p.rho, p.pr);
        maxcsqr = std::max(c * c, maxcsqr);
        maxvsqr = std::max(p.v.Dot(p.v), maxvsqr);
        maxasqr = std::max(p.a.Dot(p.a), maxasqr);
    }

    float maxv = std::max(0.000001f, std::sqrt(maxvsqr));
    float maxa = std::max(0.000001f, std::sqrt(maxasqr));
    float maxc = std::max(0.000001f, std::sqrt(maxcsqr));

    return std::max(mintimestep, std::min({ counrant_safetyconst * h / maxv, std::sqrt(h / maxa), counrant_safetyconst * h / maxc }));
}

vector3 sphfluid::center()
{
    return vector3::Zero;
}

std::vector<uint8_t> sphfluid::texturedata() const
{
    return std::vector<uint8_t>(4);
}

std::vector<gfx::vertex> sphfluid::vertices() const
{
    return particlegeometry.vertices();
}

std::vector<gfx::instance_data> sphfluid::instancedata() const
{
    std::vector<gfx::instance_data> particles_instancedata;
    for (auto const& particleparam : particleparams)
    {
        //if (particleparam.surf)
        {
            particles_instancedata.emplace_back(matrix::CreateTranslation(particleparam.p), gfx::globalresources::get().view(), gfx::globalresources::get().mat(particleparam.matname));
        }
    }

    return particles_instancedata;
}

constexpr float poly6kernelcoeff()
{
    return 315.0f/(64.0f * XM_PI * stdx::pown(h, 9u));
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

void sphfluid::update(float dt)
{
    float remainingtime = dt;
    while (remainingtime > 0.0f)
    {
        float timestep = std::min(computetimestep(), remainingtime);
        remainingtime -= timestep;

        // compute pressure and densities
        for (auto& particleparam : particleparams)
        {
            particleparam.rho = 0.0f;
            for (auto neighbourparam : particleparams)
            {
                float const distsqr = vector3::DistanceSquared(particleparam.p, neighbourparam.p);

                if (distsqr < hsqr)
                {
                    float const term = stdx::pown(std::max((hsqr - distsqr), 0.0f), 3u);
                    particleparam.rho += poly6kernelcoeff() * term;
                }
            }

            // prevent denity smaller than reference density to avoid negative pressure
            particleparam.rho = std::max(particleparam.rho, rho0);
            particleparam.pr = k * (particleparam.rho - rho0);
        }

        // compute acceleration
        for (auto& particleparam : particleparams)
        {
            auto const& v = particleparam.v;
            auto const& d = particleparam.rho;
            auto const& pr = particleparam.pr;

            particleparam.a = vector3::Zero;

            for (auto neighbourparam : particleparams)
            {
                auto const& nv = neighbourparam.v;
                auto const& nrho = neighbourparam.rho;
                auto const& npr = neighbourparam.pr;

                auto toneighbour = neighbourparam.p - particleparam.p;
                float const dist = toneighbour.Length();

                float const diff = std::max(0.0f, h - dist);

                if (diff > 0)
                {
                    toneighbour.Normalize();

                    // acceleration due to pressure
                    particleparam.a += (spikykernelcoeff() * (pr + npr) * stdx::pown(diff, 2u) / (2.0f * d * nrho)) * toneighbour;

                    // accleration due to viscosity
                    particleparam.a += (viscosityconstant * (nv - v) * viscositylaplaciancoeff() * diff) / nrho;
                }
            }

            // add gravity
            particleparam.a += vector3(0.0f, -1.0f, 0.0f);
        }

        // collisions
        // just reflect velocity
        {
            static constexpr float repulsion_force = 1.0f;
            auto const& containercenter = container.center();
            auto const& halfextents = container.span() / 2.0f;

            for (auto& particleparam : particleparams)
            {
                auto const& localpt = particleparam.p - containercenter;
                auto const localpt_abs = vector3{ std::abs(localpt.x), std::abs(localpt.y), std::abs(localpt.z) };

                vector3 normal = vector3::Zero;
                if (localpt_abs.x > halfextents.x)
                {
                    normal += -stdx::sign(localpt.x) * vector3::UnitX;
                }

                if (localpt_abs.y > halfextents.y)
                {
                    normal += -stdx::sign(localpt.y) * vector3::UnitY;
                }

                if (localpt_abs.z > halfextents.z)
                {
                    normal += -stdx::sign(localpt.z) * vector3::UnitZ;
                }

                if (normal != vector3::Zero)
                {
                    normal.Normalize();

                    auto const& penetration = localpt_abs - halfextents;
                    auto const impulsealongnormal = particleparam.v.Dot(-normal);

                    static constexpr float restitution = 0.3f;
                    particleparam.p += penetration * normal;
                    particleparam.v += (1.0f + restitution) * impulsealongnormal * normal;
                }

                if (particleparam.a.LengthSquared() > maxacc * maxacc)
                {
                    particleparam.a = particleparam.a.Normalized() * maxacc;
                }

                if (particleparam.v.LengthSquared() > maxspeed * maxspeed)
                {
                    particleparam.v = particleparam.v.Normalized() * maxspeed;
                }
            }
        }

        // compute positions
        for (auto& particleparam : particleparams)
            particleparam.vp = particleparam.v + particleparam.a * timestep;

        for (auto& particleparam : particleparams)
            particleparam.p += particleparam.vp * timestep;

        for (auto& particleparam : particleparams)
            particleparam.v = particleparam.vp + particleparam.a * (0.5f * timestep);
    }

    // compute gradient of colour field
    for (auto& particleparam : particleparams)
    {
        particleparam.gc = vector3::Zero;

        for (auto neighbourparam : particleparams)
        {
            auto toneighbour = neighbourparam.p - particleparam.p;
            float const dist = toneighbour.Length();

            float const diff = std::max(0.0f, h - dist);

            if (diff > 0)
            {
                toneighbour.Normalize();

                // gradient of colour field
                particleparam.gc += poly6gradcoeff() * stdx::pown(std::max((hsqr - diff * diff), 0.0f), 2u) * toneighbour / neighbourparam.rho;
            }
        }

        particleparam.surf = particleparam.gc.LengthSquared() > (surfthresholdsqr - stdx::tolerance<>);
    }
}

sphfluidintro::sphfluidintro(view_data const& viewdata) : sample_base(viewdata)
{
	camera.Init({ 0.f, 0.f, -30.f });
	camera.SetMoveSpeed(10.0f);
}

gfx::resourcelist sphfluidintro::create_resources()
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

    globalres.cbuffer().updateresource();

    // since these use static vertex buffers, just send 0 as maxverts
    boxes.emplace_back(cube{ vector3{0.f, 0.f, 0.f}, vector3{roomextents} }, &cube::vertices_flipped, &cube::instancedata, bodyparams{ 0, 1, "instanced" });
    fluid.emplace_back(sphfluid(boxes[0]->bbox()), bodyparams{ 0, numparticles, "instanced" });

    gfx::resourcelist res;
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, fluid)) { stdx::append(b->create_resources(), res); };
    return res;
}

void sphfluidintro::update(float dt)
{
    sample_base::update(dt);
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, fluid)) b->update(dt);
}

void sphfluidintro::render(float dt)
{
    for (auto b : stdx::makejoin<gfx::bodyinterface>(boxes, fluid)) b->render(dt, { false });
}
