module;

#define NOMINMAX
#include <windows.h>
#include <wrl.h>
#include "simplemath/simplemath.h"

export module engineutils;

import std;
import stdxcore;
import vec;

export using Microsoft::WRL::ComPtr;

export namespace utils
{

// helper function until matrix4x4 is better implemented 
stdx::matrix4x4 to_matrix4x4(DirectX::SimpleMath::Matrix const& matrix)
{
    stdx::matrix4x4 r;
    r[0] = stdx::vec4{ matrix.m[0][0], matrix.m[0][1], matrix.m[0][2], matrix.m[0][3] };
    r[1] = stdx::vec4{ matrix.m[1][0], matrix.m[1][1], matrix.m[1][2], matrix.m[1][3] };
    r[2] = stdx::vec4{ matrix.m[2][0], matrix.m[2][1], matrix.m[2][2], matrix.m[2][3] };
    r[3] = stdx::vec4{ matrix.m[3][0], matrix.m[3][1], matrix.m[3][2], matrix.m[3][3] };

    return r;
}

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