module;

#define __SPECSTRINGS_STRICT_LEVEL 0
#include "simplemath/simplemath.h"

module cursor;

using viewport = DirectX::SimpleMath::Viewport;

namespace continuity
{

void cursor::tick(float dt)
{
    _lastpos = _pos;
    _pos = pos();
    _vel = (_pos - _lastpos) / dt;
    _vel[1] = -_vel[1];     // pos is relative to topleft so invert y
}

veci2 cursor::posicentered() const
{
    POINT pos;
    GetCursorPos(&pos);
    ScreenToClient(GetActiveWindow(), &pos);

    RECT clientr;
    GetClientRect(GetActiveWindow(), &clientr);

    int const width = static_cast<int>(clientr.right - clientr.left);
    int const height = static_cast<int>(clientr.bottom - clientr.top);

    return { pos.x - width / 2, height / 2 - pos.y };
}


vec2 cursor::pos() const
{
    POINT pos;
    GetCursorPos(&pos);
    ScreenToClient(GetActiveWindow(), &pos);
    return { static_cast<float>(pos.x), static_cast<float>(pos.y) };
}

vec2 cursor::vel() const { return _vel; }

line cursor::ray(float nearp, float farp, matrix const& view, matrix const& proj) const
{
    auto const raystart = to3d({ _pos[0], _pos[1], nearp}, nearp, farp, view, proj);
    auto const rayend = to3d({ _pos[0], _pos[1], farp}, nearp, farp, view, proj);

    return line(raystart, (rayend - raystart).normalized());
}

vec3 cursor::to3d(vec3 pos, float nearp, float farp, matrix const& view, matrix const& proj) const
{
    RECT clientr;
    GetClientRect(GetActiveWindow(), &clientr);

    float const width = static_cast<float>(clientr.right - clientr.left);
    float const height = static_cast<float>(clientr.bottom - clientr.top);

    // convert to ndc [-1, 1]
    vector4 const ndc = vector4{ pos[0] / width, (height - pos[1] - 1.f) / height, (pos[2] - nearp) / (farp - nearp) , 1.f} *2.f - vector4{1.f};

    // homogenous space
    auto posh = vector4::Transform(ndc, proj.Invert());
    posh /= posh.w;

    // world space
    posh = vector4::Transform(posh, view.Invert());

    return { posh.x, posh.y, posh.z };
}

}