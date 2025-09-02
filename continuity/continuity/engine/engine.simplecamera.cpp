module;

#define NOMINMAX
#include <wtypes.h>
#include <DirectXMath.h>
#include "simplemath/simplemath.h"

module engine:simplecamera;

import std;

using namespace DirectX;

using vector3 = DirectX::SimpleMath::Vector3;
using vector4 = DirectX::SimpleMath::Vector4;
using matrix = DirectX::SimpleMath::Matrix;

static constexpr XMFLOAT3 defaultlook_dir = { 0.f, 0.f, 1.f };

simplecamera::simplecamera() :
    m_initialPosition{ 0, 0, 0 },
    m_position{ m_initialPosition },
    m_yaw(0),
    m_pitch(0.0f),
    m_lookDirection(defaultlook_dir),
    m_upDirection(0, 1, 0),
    m_moveSpeed(20.0f),
    m_turnSpeed(XM_PIDIV2),
    m_keysPressed{}
{
}

void simplecamera::Init(stdx::vec3 position)
{
    m_initialPosition = position;
    m_cameramatx.Translation(vector3(&m_initialPosition[0]));
    Reset();
}

void simplecamera::SetMoveSpeed(float unitsPerSecond)
{
    m_moveSpeed = unitsPerSecond;
}

void simplecamera::SetTurnSpeed(float radiansPerSecond)
{
    m_turnSpeed = radiansPerSecond;
}

void simplecamera::Reset()
{
    m_position = m_initialPosition;
    m_cameramatx.Translation(vector3(&m_position[0]));
    m_yaw = 0;
    m_pitch = 0.0f;
    m_lookDirection = { defaultlook_dir };
}

void simplecamera::Update(float elapsedSeconds)
{
    POINT pos;
    RECT client;
    GetCursorPos(&pos);
    ScreenToClient(m_window, &pos);
    GetClientRect(m_window, &client);

    stdx::veci2 topleft{ client.left, client.top }, botright{ client.right, client.bottom };
    stdx::vec2 clientsz = (botright - topleft).castas<float>();
    stdx::vec2 center = clientsz / 2;

    // normalized cursor pos
    cursorpos = (stdx::vec2{ float(pos.x), float(pos.y) } - center) / clientsz;

    auto deltacursor = (cursorpos - lastcursorpos) * 2.0f;

    if (_locked) return;

    if (GetKeyState(VK_LBUTTON) < 0 && GetActiveWindow() == m_window)
    {
        m_yaw += deltacursor[0];
        m_pitch += deltacursor[1];
    }

    // Calculate the move vector in camera space.
    XMFLOAT3 move(0, 0, 0);

    if (m_keysPressed.a)
        move.x -= 1.0f;
    if (m_keysPressed.d)
        move.x += 1.0f;
    if (m_keysPressed.w)
        move.z += 1.0f;
    if (m_keysPressed.s)
        move.z -= 1.0f;

    if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f)
    {
        XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&move));
        move.x = XMVectorGetX(vector);
        move.z = XMVectorGetZ(vector);
    }

    float moveInterval = m_moveSpeed * elapsedSeconds;
    float rotateInterval = m_turnSpeed * elapsedSeconds;

    if (m_keysPressed.left)
        m_yaw += rotateInterval;
    if (m_keysPressed.right)
        m_yaw -= rotateInterval;
    if (m_keysPressed.up)
        m_pitch -= rotateInterval;
    if (m_keysPressed.down)
        m_pitch += rotateInterval;

    m_pitch = std::min(m_pitch, (XM_PIDIV2 - 0.00001f));
    m_pitch = std::max((-XM_PIDIV2 + 0.00001f), m_pitch);

    if (m_yaw > XM_PI)
        m_yaw -= XM_2PI;
    else if (m_yaw <= -XM_PI)
        m_yaw += XM_2PI;

    vector3 camfwd;
    // this is slighty different from convential construction, but this generates +z when yaw = 0
    camfwd.x = -(std::sin(m_yaw) * cos(m_pitch));
    camfwd.y = -std::sin(m_pitch);
    camfwd.z = std::cos(m_yaw) * std::cos(m_pitch);

    camfwd.Normalize();

    vector3 campos = vector3(m_position[0], m_position[1], m_position[2]);

    vector3 camright = vector3::UnitY.Cross(camfwd);

    campos += camfwd * move.z * moveInterval + camright * move.x * moveInterval;

    // this constructs a world to view matrix
    m_cameramatx = XMMatrixLookToLH(campos, camfwd, vector3::UnitY);

    m_position = stdx::vec3{ campos.x, campos.y, campos.z };

    lastcursorpos = cursorpos;
}

stdx::vec3 simplecamera::GetCurrentPosition() const
{
    return m_position;
}

XMMATRIX simplecamera::GetViewMatrix()
{
    return m_cameramatx;
}

XMMATRIX simplecamera::GetProjectionMatrix()
{
    auto const projmat = XMMatrixPerspectiveFovLH(XM_PI / 3.0f, static_cast<float>(_width) / static_cast<float>(_height), _nearp, _farp);
    auto const jittermat = XMMatrixTranslation(subpixeljitter[0], subpixeljitter[1], 0.0f);
    auto jittredproj = XMMatrixMultiply(projmat, jittermat);

    return jittredproj;
}

DirectX::XMMATRIX simplecamera::GetOrthoProjectionMatrix()
{
    return XMMatrixOrthographicLH(static_cast<float>(_width), static_cast<float>(_height), _nearp, _farp);
}

void simplecamera::lock(bool lock) { _locked = lock; }

void simplecamera::jitter(stdx::vec2 jitter) { subpixeljitter = jitter; }

float simplecamera::nearplane() const { return _nearp; }

float simplecamera::farplane() const { return _farp; }

void simplecamera::nearplane(float nearp) { _nearp = nearp; }

void simplecamera::farplane(float farp) { _farp = farp; }

void simplecamera::width(unsigned width) { _width = width; }

void simplecamera::height(unsigned height) { _height = height; }

HWND& simplecamera::window() { return m_window; }

void simplecamera::TopView()
{
    m_position = { 0, 3, 0.f };
    m_yaw = 0.f;
    m_pitch = -XM_PIDIV2;
    m_lookDirection = { 0, -1, 0 };
}

void simplecamera::BotView()
{
    m_position = { 0, -3, 0.f };
    m_yaw = 0.f;
    m_pitch = XM_PIDIV2;
    m_lookDirection = { 0, 1, 0 };
}

void simplecamera::OnKeyDown(WPARAM key)
{
    switch (key)
    {
    case 'W':
        m_keysPressed.w = true;
        break;
    case 'A':
        m_keysPressed.a = true;
        break;
    case 'S':
        m_keysPressed.s = true;
        break;
    case 'D':
        m_keysPressed.d = true;
        break;
    case VK_LEFT:
        m_keysPressed.left = true;
        break;
    case VK_RIGHT:
        m_keysPressed.right = true;
        break;
    case VK_UP:
        m_keysPressed.up = true;
        break;
    case VK_DOWN:
        m_keysPressed.down = true;
        break;
    case VK_ESCAPE:
        Reset();
        break;
    }
}

void simplecamera::OnKeyUp(WPARAM key)
{
    switch (key)
    {
    case 'W':
        m_keysPressed.w = false;
        break;
    case 'A':
        m_keysPressed.a = false;
        break;
    case 'S':
        m_keysPressed.s = false;
        break;
    case 'D':
        m_keysPressed.d = false;
        break;
    case VK_LEFT:
        m_keysPressed.left = false;
        break;
    case VK_RIGHT:
        m_keysPressed.right = false;
        break;
    case VK_UP:
        m_keysPressed.up = false;
        break;
    case VK_DOWN:
        m_keysPressed.down = false;
        break;
    }
}
