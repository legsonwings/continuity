module;

#include <windows.h>
#include <strsafe.h>

module engine;

import activesample;
import std;

using Microsoft::WRL::ComPtr;

sample_base::sample_base(view_data const& data) : viewdata(data)
{
    camera.nearplane(data.nearplane);
    camera.farplane(data.farplane);
    camera.width(data.width);
    camera.height(data.height);
}

void sample_base::onwindowcreated()
{
    camera.window() = continuity::GetHwnd();
}

void sample_base::updateview(float dt)
{
    camera.Update(dt);

    gfx::globalresources::get().view().view = camera.GetViewMatrix();
    gfx::globalresources::get().view().proj = camera.GetProjectionMatrix();
}

continuity::continuity()
    : m_width(2560)
    , m_height(1440)
    , m_frameCounter(0)
{
    view_data vdata;
    vdata.width = m_width;
    vdata.height = m_height;
    sample = sample_creator::create_instance<activesample>(vdata);
}

void continuity::OnInit()
{
    renderer.init(GetHwnd(), m_width, m_height);

    // need to keep these alive till data is uploaded to gpu
    std::vector<ComPtr<ID3D12Resource>> const gpu_resources = sample->create_resources(renderer);

    // this will block until resources are uploaded to the gpu
    renderer.createresources();
}

void continuity::OnUpdate()
{
    m_timer.Tick(NULL);

    if (m_frameCounter++ % 30 == 0)
    {
        // update window text with FPS value.
        wchar_t fps[64];
        swprintf_s(fps, L"%ufps", m_timer.GetFramesPerSecond());

        std::wstring title = sample_titles[int(activesample)];
        if (title.empty())
            title = L"continuity";

        title = title + L": " + fps;
        SetCustomWindowText(title);
    }

    // update render target for current frame
    sample->update(static_cast<float>(m_timer.GetElapsedSeconds()));
}

// ender the scene.
void continuity::OnRender()
{
    renderer.prerender();
    sample->render(static_cast<float>(m_timer.GetElapsedSeconds()), renderer);
    renderer.postrender();
}

void continuity::OnDestroy()
{
    renderer.deinit();
}

void continuity::OnKeyDown(UINT8 key)
{
    sample->on_key_down(key);
}

void continuity::OnKeyUp(UINT8 key)
{
    sample->on_key_up(key);
}

// helper function for setting the window's title text.
void continuity::SetCustomWindowText(std::wstring const& text)
{
    std::wstring windowText = m_title + L": " + text;
    SetWindowText(continuity::GetHwnd(), windowText.c_str());
}

int continuity::Run(continuity* pSample, HINSTANCE hInstance, int nCmdShow)
{
    // parse the command line parameters
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    //pSample->ParseCommandLineArgs(argv, argc);
    LocalFree(argv);

    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"DXSampleClass";
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, static_cast<LONG>(pSample->GetWidth()), static_cast<LONG>(pSample->GetHeight()) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        pSample->GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // we have no parent window.
        nullptr,        // we aren't using menus.
        hInstance,
        pSample);

    pSample->sample->onwindowcreated();

    // initialize the sample. OnInit is defined in each child-implementation of DXSample.
    pSample->OnInit();

    ShowWindow(m_hwnd, nCmdShow);

    // Main sample loop.
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    pSample->OnDestroy();

    // return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}

// main message handler for the sample.
LRESULT CALLBACK continuity::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    continuity* pSample = reinterpret_cast<continuity*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        // save the sample object pointer passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
    return 0;

    case WM_KEYDOWN:
        if (pSample)
        {
            pSample->OnKeyDown(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_KEYUP:
        if (pSample)
        {
            pSample->OnKeyUp(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_PAINT:
        if (pSample)
        {
            pSample->OnUpdate();
            pSample->OnRender();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    // handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}
