#define NOMINMAX
#include <windows.h>

import engine;

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    continuity sample{};

    return continuity::Run(&sample, hInstance, nCmdShow);
}
