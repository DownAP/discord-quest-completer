#pragma once
#include <functional>

struct RenderOptions
{
    const wchar_t* title = L"Window";
    int  width     = 760;
    int  height    = 600;
    bool resizable = true;
};

int RunImGuiWindow(const RenderOptions& opts, const std::function<bool()>& frame);
