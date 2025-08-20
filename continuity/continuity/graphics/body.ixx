module;

#include <wrl.h>
#include <d3d12.h>
#include "shared/sharedconstants.h"
#include "simplemath/simplemath.h"
#include "shared/common.h"

export module body;

import vec;
import stdxcore;
import geometry;
import graphicscore;
import graphics;

import std;

using Microsoft::WRL::ComPtr;

using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;

using vec2 = stdx::vec2;
using vec3 = stdx::vec3;
using veci2 = stdx::veci2;
using line = geometry::line;

export namespace gfx
{

struct instances_data
{
    std::vector<instance_data> data;
};

template<typename t>
concept hasverticesold = requires(t v)
{
    { v.vertices() } -> std::convertible_to<std::vector<vertex>>;
};

template<typename t>
concept hasvertices = requires(t v)
{
    { v.vertices() } -> std::convertible_to<vertexattribs>;
};

template<typename t>
concept hasupdate = requires(t v)
{
    v.update(float{});
};

template<typename t>
concept hasinstancedata = requires(t v)
{
    { v.instancedata() } -> std::convertible_to<std::vector<instance_data>>;
};

template<typename t>
concept hastexturedata = requires(t v)
{
    { v.texturedata() } -> std::same_as<std::vector<uint8_t>>;
};

template <typename t>
concept sbodyraw_c = hasvertices<t> && hasinstancedata<t>;

template <typename t>
concept dbodyraw_c = hasverticesold<t> && hasupdate<t> && hastexturedata<t>;

template <typename t>
concept sbody_c = (sbodyraw_c<t> || ((stdx::lvaluereference_c<t> || stdx::rvaluereference_c<t>) && sbodyraw_c<std::decay<t>>));

template <typename t>
concept dbody_c = (dbodyraw_c<t> || ((stdx::lvaluereference_c<t> || stdx::rvaluereference_c<t>) && dbodyraw_c<std::decay<t>>));

struct renderparams;

struct bodyparams
{
    uint maxverts;
    uint maxinstances;

    stdx::vecui2 dims;
};

class bodyinterface
{
    bodyparams params;
public:
    bodyinterface(bodyparams const& _bodyparams) : params(_bodyparams) {}

    bodyparams const& getparams() const { return params; }
    virtual void update(float dt) {};
    virtual void render(float dt, renderparams const&) {};
    virtual gfx::resourcelist create_resources() { return {}; };
    virtual D3D12_GPU_VIRTUAL_ADDRESS get_vertexbuffer_gpuaddress() const { return 0; };
};

template<topology prim_t = topology::triangle>
struct topologyconstants
{
    using vertextype = gfx::vertex;
    static constexpr uint32_t numverts_perprim = 3;
    static constexpr uint32_t maxprims_permsgroup = MAX_TRIANGLES_PER_GROUP;
};

template<>
struct topologyconstants<topology::line>
{
    using vertextype = vector3;
    static constexpr uint32_t numverts_perprim = 2;
    static constexpr uint32_t maxprims_permsgroup = MAX_LINES_PER_GROUP;
};

template<sbody_c body_t, topology prim_t>
class body_static : public bodyinterface
{
    using rawbody_t = std::decay_t<body_t>;
    using vertextype = typename topologyconstants<prim_t>::vertextype;

    uint32 _descriptorsindex;
    body_t body;
    structuredbuffer<instance_data, accesstype::both> _objconstants;
    structuredbuffer<dispatchparams, accesstype::both> _dispatchparams;
    structuredbuffer<stdx::vec3, accesstype::both > _posbuffer;
    structuredbuffer<vector2, accesstype::both> _texcoordbuffer;
    structuredbuffer<tbn, accesstype::both> _tbnbuffer;
    structuredbuffer<index, accesstype::both> _indexbuffer;
    structuredbuffer<uint32, gfx::accesstype::both> _materialsbuffer;

    using vertexfetch_r = vertexattribs;
    using indexfetch_r = std::vector<index>;
    using vertexfetch = std::function<vertexfetch_r(rawbody_t const&)>;
    using indexfetch = std::function<indexfetch_r(rawbody_t const&)>;
    using instancedatafetch_r = std::vector<instance_data>;
    using instancedatafetch = std::function<instancedatafetch_r(rawbody_t const&)>;

    vertexfetch get_vertices;
    indexfetch get_indices;
    instancedatafetch get_instancedata;

public:

    template<typename body_c_t = body_t>
    body_static(body_c_t&& _body, bodyparams const& _params);

    template<typename body_c_t = body_t>
    body_static(body_c_t&& _body, vertexfetch_r(rawbody_t::* vfun)() const, instancedatafetch_r(rawbody_t::* ifun)() const, bodyparams const& _params);

    gfx::resourcelist create_resources() override;
    void update(float dt) override;

    uint32 descriptorsindex() const { return _descriptorsindex; }
	uint32 numprims() const { return uint32(_indexbuffer.numelements / topologyconstants<prim_t>::numverts_perprim); }

    constexpr body_t& get() { return body; }
    constexpr body_t const& get() const { return body; }
    constexpr body_t& operator*() { return body; }
    constexpr body_t const& operator*() const { return body; }
    constexpr rawbody_t* operator->() { return &body; }
    constexpr rawbody_t const* operator->() const { return &body; }
};

template<dbody_c body_t, topology prim_t>
class body_dynamic : public bodyinterface
{
    using rawbody_t = std::decay_t<body_t>;
    using vertextype = typename topologyconstants<prim_t>::vertextype;

    body_t body;
    dynamicbuffer<vertextype> _vertexbuffer;
    texture_dynamic _texture{ DXGI_FORMAT_R8G8B8A8_UNORM };

    using vertexfetch_r = std::vector<vertextype>;
    using vertexfetch = std::function<vertexfetch_r(rawbody_t const&)>;

    vertexfetch get_vertices;

public:

    template<typename body_c_t = body_t>
    body_dynamic(body_c_t&& _body, bodyparams const& _params, stdx::vecui2 texdims = {});

    template<typename body_c_t = body_t>
    body_dynamic(body_c_t&& _body, vertexfetch_r(rawbody_t::* fun)() const, bodyparams const& _params, stdx::vecui2 texdims = {});

    gfx::resourcelist create_resources() override;

    void update(float dt) override;
    void render(float dt, renderparams const&) override;

    constexpr body_t& get() { return body; }
    constexpr body_t const& get() const { return body; }
    constexpr body_t& operator*() { return body; }
    constexpr body_t const& operator*() const { return body; }
    constexpr rawbody_t* operator->() { return &body; }
    constexpr rawbody_t const* operator->() const { return &body; }
};

// bodyimpl

template<sbody_c body_t, topology prim_t>
template<typename body_c_t>
inline body_static<body_t, prim_t>::body_static(body_c_t&& _body, bodyparams const& _params) : bodyinterface(_params), body(std::forward<body_c_t>(_body))
{
    get_vertices = [](body_t const& geom) { return geom.vertices(); };
    get_instancedata = [](body_t const& geom) { return geom.instancedata(); };
    get_indices = [](body_t const& geom) { return geom.indices(); };
}

template<sbody_c body_t, topology prim_t>
template<typename body_c_t>
inline body_static<body_t, prim_t>::body_static(body_c_t&& _body, vertexfetch_r(rawbody_t::* vfun)() const, instancedatafetch_r(rawbody_t::* ifun)() const, bodyparams const& _params) : bodyinterface(_params), body(std::forward<body_c_t>(_body))
{
    get_vertices = [vfun](body_t const& geom) { return std::invoke(vfun, geom); };
    get_instancedata = [ifun](body_t const& geom) { return std::invoke(ifun, geom); };

    // todo : no function for indices yet
    get_indices = [](body_t const& geom) { return geom.indices(); };
}

template<sbody_c body_t, topology prim_t>
std::vector<ComPtr<ID3D12Resource>> body_static<body_t, prim_t>::create_resources()
{
    std::vector<ComPtr<ID3D12Resource>> res;

    auto const& vertices = get_vertices(body);
    auto const& materials = body.materials();

    // better to create static vertex buffers in default heap
    _posbuffer.create(vertices.positions);
    _texcoordbuffer.create(vertices.texcoords);
    _tbnbuffer.create(vertices.tbns);

    _indexbuffer.create(get_indices(body));
    _materialsbuffer.create(materials);
    //_instancebuffer.createresource(getparams().maxinstances);

    auto bodydata = get_instancedata(body);

    // only one instance right now
    _objconstants.create(bodydata);

    dispatchparams dispatch_params;
    dispatch_params.numverts_perprim = topologyconstants<prim_t>::numverts_perprim;
    dispatch_params.numprims_perinstance = static_cast<uint32>(_indexbuffer.numelements / topologyconstants<prim_t>::numverts_perprim);
    dispatch_params.numprims = static_cast<uint32>(dispatch_params.numprims_perinstance);
    dispatch_params.maxprims_permsgroup = topologyconstants<prim_t>::maxprims_permsgroup;
    dispatch_params.posbuffer = _posbuffer.createsrv().heapidx;
    dispatch_params.texcoordbuffer = _texcoordbuffer.createsrv().heapidx;
    dispatch_params.tbnbuffer = _tbnbuffer.createsrv().heapidx;
    dispatch_params.indexbuffer = _indexbuffer.createsrv().heapidx;
    dispatch_params.materialsbuffer = _materialsbuffer.createsrv().heapidx;
    dispatch_params.objconstants = _objconstants.createsrv().heapidx;

    _dispatchparams.create({ dispatch_params });

    _descriptorsindex = _dispatchparams.createsrv().heapidx;

    // return any upload buffers so that engine can keep it alive until data has been uploaded to gpu
    return res;
}

template<sbody_c body_t, topology prim_t>
void body_static<body_t, prim_t>::update(float dt)
{
    // update only if we own this body
    if constexpr (std::is_same_v<body_t, rawbody_t> && hasupdate<rawbody_t>) body.update(dt);

    auto bodydata = get_instancedata(body);

    // only one instance right now 
    _objconstants.update(bodydata);
}

template<dbody_c body_t, topology prim_t>
template<typename body_c_t>
inline body_dynamic<body_t, prim_t>::body_dynamic(body_c_t&& _body, bodyparams const& _params, stdx::vecui2 texdims) : bodyinterface(_params), body(std::forward<body_c_t>(_body))
{
    _texture._dims = texdims;
    get_vertices = [](body_t const& geom) { return geom.vertices(); };
}

template<dbody_c body_t, topology prim_t>
template<typename body_c_t>
inline body_dynamic<body_t, prim_t>::body_dynamic(body_c_t&& _body, vertexfetch_r(rawbody_t::* fun)() const, bodyparams const& _params, stdx::vecui2 texdims) : bodyinterface(_params), body(std::forward<body_c_t>(_body))
{
    _texture._dims = texdims;
    get_vertices = [fun](body_t const& geom) { return std::invoke(fun, geom); };
}

template<dbody_c body_t, topology prim_t>
inline std::vector<ComPtr<ID3D12Resource>> body_dynamic<body_t, prim_t>::create_resources()
{
    //_cbuffer.createresource();
    auto const& verts = get_vertices(body);
    _vertexbuffer.createresource(getparams().maxverts);
    _texture.createresource(getparams().dims, body.texturedata());

    stdx::cassert(_vertexbuffer.count() < ASGROUP_SIZE * MAX_MSGROUPS_PER_ASGROUP * topologyconstants<prim_t>::maxprims_permsgroup * topologyconstants<prim_t>::numverts_perprim);
    return {};
}

template<dbody_c body_t, topology prim_t>
inline void body_dynamic<body_t, prim_t>::update(float dt)
{
    // update only if we own this body
    if constexpr (std::is_same_v<body_t, rawbody_t>) body.update(dt);
}

template<dbody_c body_t, topology prim_t>
inline void body_dynamic<body_t, prim_t>::render(float dt, renderparams const& params)
{
    //auto const foundpso = globalresources::get().psomap().find(params.psoname);
    //if (foundpso == globalresources::get().psomap().cend())
    //{
    //    stdx::cassert(false, "pso not found");
    //    return;
    //}

    _vertexbuffer.updateresource(get_vertices(body));
    _texture.updateresource(body.texturedata());
    //_cbuffer.updateresource(objectconstants(matrix::CreateTranslation(body.center()), globalresources::get().view()));
    stdx::cassert(_vertexbuffer.count() < ASGROUP_SIZE * MAX_MSGROUPS_PER_ASGROUP * topologyconstants<prim_t>::maxprims_permsgroup * topologyconstants<prim_t>::numverts_perprim);

    //dispatchparams dispatch_params;
    //dispatch_params.numverts_perprim = topologyconstants<prim_t>::numverts_perprim;
    //dispatch_params.numprims = static_cast<uint32_t>(_vertexbuffer.count() / topologyconstants<prim_t>::numverts_perprim);
    //dispatch_params.maxprims_permsgroup = topologyconstants<prim_t>::maxprims_permsgroup;
    //struct resource_bindings
    //{
    //    rootbuffer constant;
    //    rootbuffer objectconstant;
    //    rootbuffer vertex;
    //    rootbuffer instance;
    //    rootbuffer customdata;
    //    buffer texture;
    //    rootconstants rootconstants;
    //    pipeline_objects pipelineobjs;
    //};

    //resource_bindings bindings;

    ////bindings.constant = { 0, globalresources::get().cbuffer().currframe_gpuaddress() };
    ////bindings.objectconstant = { 1, _cbuffer.currframe_gpuaddress() };
    ////bindings.vertex = { 3, _vertexbuffer.gpuaddress() };
    //bindings.pipelineobjs = foundpso->second;
    //bindings.rootconstants.slot = 0;
    ////bindings.rootconstants.values.resize(sizeof(dispatch_params) / sizeof(uint32));

    //if (_texture.size() > 0)
    //    bindings.texture = { 5, _texture.deschandle() };

    //uint const numasthreads = static_cast<uint>(std::ceil(static_cast<float>(dispatch_params.numprims) / static_cast<float>(ASGROUP_SIZE * dispatch_params.maxprims_permsgroup)));
    //stdx::cassert(numasthreads < 128);
    ////memcpy(bindings.rootconstants.values.data(), &dispatch_params, sizeof(dispatch_params));
    //dispatch(bindings, params.wireframe, false, numasthreads);
}
// bodyimpl

}
