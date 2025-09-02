module;

#define NOMINMAX
#include <wtypes.h>
#include <DirectXMath.h>
#include "simplemath/simplemath.h"

export module engine:simplecamera;

import vec;

class simplecamera
{
public:
    simplecamera();

    void Init(stdx::vec3 position);
    void Update(float elapsedSeconds);
    stdx::vec3 GetCurrentPosition() const;
    DirectX::XMMATRIX GetViewMatrix();
    DirectX::XMMATRIX GetProjectionMatrix();
    DirectX::XMMATRIX GetOrthoProjectionMatrix();

    void lock(bool lock);
    void jitter(stdx::vec2 jitter);

    float nearplane() const;
    float farplane() const;
    void nearplane(float nearp);
    void farplane(float farp);
    void width(unsigned width);
    void height(unsigned height);
    HWND& window();

    void SetMoveSpeed(float unitsPerSecond);
    void SetTurnSpeed(float radiansPerSecond);

    void TopView();
    void BotView();
    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);
private:
    void Reset();

    struct KeysPressed
    {
        bool w;
        bool a;
        bool s;
        bool d;

        bool left;
        bool right;
        bool up;
        bool down;
    };

    stdx::vec3 m_initialPosition;
    stdx::vec3 m_position;
    float m_yaw;                // Relative to the +z axis.
    float m_pitch;                // Relative to the xz plane.
    DirectX::XMFLOAT3 m_lookDirection;
    DirectX::XMFLOAT3 m_upDirection;
    float m_moveSpeed;            // Speed at which the camera moves, in units per second.
    float m_turnSpeed;            // Speed at which the camera turns, in radians per second.

    DirectX::SimpleMath::Matrix m_cameramatx;

    bool _locked = false;
    float _nearp = std::numeric_limits<float>::max();
    float _farp = 0.0001f;
    unsigned _width = 0;
    unsigned _height = 0;
    stdx::vec2 subpixeljitter = {};

    KeysPressed m_keysPressed;

    stdx::vec2 cursorpos{0, 0};
    stdx::vec2 lastcursorpos{0, 0};

    HWND m_window = nullptr;
};
