#define NOMINMAX
#include <windows.h>
#include <dxgidebug.h>
#include <wrl.h>
#include <dxgi1_6.h>

import engine;

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    int retcode = 0;
    {
        continuity sample{};

        retcode = continuity::Run(&sample, hInstance, nCmdShow);
    }

#ifdef _DEBUG
    Microsoft::WRL::ComPtr<IDXGIDebug1> dxgidebug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgidebug.GetAddressOf()))))
    {
        dxgidebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    }
#endif

    return retcode;
}
