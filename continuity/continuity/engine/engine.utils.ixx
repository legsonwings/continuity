module;

#define NOMINMAX
#include <windows.h>

#include <wrl.h>

export module engine:utils;

import std;
import stdxcore;

export using Microsoft::WRL::ComPtr;

export namespace utils
{

std::wstring strtowstr(std::string const& str)
{
    static constexpr uint maxwchars = 100;

    if (str.empty())
    {
        return {};
    }

    if (str.size() >= maxwchars)
    {
        stdx::cassert(false, "string is too long");
    }

    wchar_t wstr[maxwchars];
    int ret = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, str.data(), -1, wstr, maxwchars);
    stdx::cassert(ret != 0, "string to wstring conversion failed. Is the input string too long?");

    return std::wstring(wstr);
}

}