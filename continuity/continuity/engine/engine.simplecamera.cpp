module;

#define NOMINMAX
#include <wtypes.h>
#include <DirectXMath.h>

module engine:simplecamera;

import std;

using namespace DirectX;

static constexpr XMFLOAT3 defaultlook_dir = { 0.f, 0.f, 1.f };

simplecamera::simplecamera() :
    m_initialPosition{ 0, 0, 0 },
    m_position{ m_initialPosition },
    m_yaw(XM_PI),
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
    m_yaw = 0.f;
    m_pitch = 0.0f;
    m_lookDirection = { defaultlook_dir };
}

void simplecamera::Update(float elapsedSeconds)
{
    if (_locked) return;

    // Calculate the move vector in camera space.
    XMFLOAT3 move(0, 0, 0);

    if (m_keysPressed.a)
        move.x -= 1.0f;
    if (m_keysPressed.d)
        move.x += 1.0f;
    if (m_keysPressed.w)
        move.z -= 1.0f;
    if (m_keysPressed.s)
        move.z += 1.0f;

    if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f)
    {
        XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&move));
        move.x = XMVectorGetX(vector);
        move.z = XMVectorGetZ(vector);
    }

    float moveInterval = m_moveSpeed * elapsedSeconds;
    float rotateInterval = m_turnSpeed * elapsedSeconds;

    if (m_keysPressed.left)
        m_yaw -= rotateInterval;
    if (m_keysPressed.right)
        m_yaw += rotateInterval;
    if (m_keysPressed.up)
        m_pitch += rotateInterval;
    if (m_keysPressed.down)
        m_pitch -= rotateInterval;

    // Prevent looking too far up or down.
    m_pitch = std::min(m_pitch, XM_PIDIV4);
    m_pitch = std::max(-XM_PIDIV4, m_pitch);

    // Move the camera in model space.
    float x = move.x * cosf(m_yaw) - move.z * sinf(m_yaw);
    float z = -move.x * sinf(m_yaw) - move.z * cosf(m_yaw);
    m_position[0] += x * moveInterval;
    m_position[2] += z * moveInterval;

    // Determine the look direction.
    float r = cosf(m_pitch);
    m_lookDirection.x = r * sinf(m_yaw);
    m_lookDirection.y = sinf(m_pitch);
    m_lookDirection.z = r * cosf(m_yaw);
}

stdx::vec3 simplecamera::GetCurrentPosition() const
{
    return m_position;
}

XMMATRIX simplecamera::GetViewMatrix()
{
    XMFLOAT3 pos = { m_position[0], m_position[1], m_position[2] };
    return XMMatrixLookToLH(XMLoadFloat3(&pos), XMLoadFloat3(&m_lookDirection), XMLoadFloat3(&m_upDirection));
}

XMMATRIX simplecamera::GetProjectionMatrix(float fov)
{
    return XMMatrixPerspectiveFovLH(fov, static_cast<float>(_width) / static_cast<float>(_height), _nearp, _farp);
}

DirectX::XMMATRIX simplecamera::GetOrthoProjectionMatrix()
{
    return XMMatrixOrthographicLH(static_cast<float>(_width), static_cast<float>(_height), _nearp, _farp);
}

void simplecamera::lock(bool lock) { _locked = lock; }

float simplecamera::nearplane() const { return _nearp; }

float simplecamera::farplane() const { return _farp; }

void simplecamera::nearplane(float nearp) { _nearp = nearp; }

void simplecamera::farplane(float farp) { _farp = farp; }

void simplecamera::width(unsigned width) { _width = width; }

void simplecamera::height(unsigned height) { _height = height; }

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
