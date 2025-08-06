module;

#include <windows.h>

export module engine;
export import :utils;
export import :steptimer;
export import :simplecamera;

import stdxcore;
import graphics;
import graphicscore;


export struct view_data
{
    unsigned width = 3840;
    unsigned height = 2160;
    float nearplane = 8000;
    float farplane = 0.0001f;

    float get_aspect_ratio() const { return static_cast<float>(width) / static_cast<float>(height); }
};

// abstract base demo class
export class sample_base
{
public:
    sample_base(view_data const& data);

    virtual ~sample_base() {}

    virtual gfx::resourcelist create_resources() = 0;

    virtual void update(float dt) { updateview(dt); };
    virtual void render(float dt, gfx::renderer& renderer) = 0;

    void onwindowcreated();
    virtual void on_key_down(unsigned key) { camera.OnKeyDown(key); };
    virtual void on_key_up(unsigned key) { camera.OnKeyUp(key); };

protected:
    simplecamera camera;
    view_data viewdata;

    void updateview(float dt);
};

export class continuity
{
public:
    continuity(view_data const& data = {});

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);

    static int Run(continuity* pSample, HINSTANCE hInstance, int nCmdShow);
    static HWND GetHwnd() { return m_hwnd; }

private:
    
    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }

    const WCHAR* GetTitle() const { return m_title.c_str(); }

    void SetCustomWindowText(std::wstring const& text);

    steptimer m_timer;
    std::unique_ptr<sample_base> sample;
    gfx::renderer renderer;
    unsigned m_frameCounter;
    
    // viewport dimensions
    UINT m_width;
    UINT m_height;
    std::wstring m_title;

    static inline HWND m_hwnd = nullptr;

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};