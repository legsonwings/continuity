#define NOMINMAX
#include <windows.h>
#include <dxgidebug.h>
#include <wrl.h>
#include <dxgi1_6.h>

import engine;

using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{

#if (_WIN32_WINNT >= 0x0A00 /*_WIN32_WINNT_WIN10*/)
    RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialize))
        return -1;
#else
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
        return -1;
#endif

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
