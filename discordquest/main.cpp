#include "ui.h"

#include <windows.h>
#include <shellapi.h>
#include <cstdlib>

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    long long durationMs = 0;

    int     argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv)
    {
        if (argc >= 2)
            durationMs = _wtoi64(argv[1]);
        ::LocalFree(argv);
    }

    if (durationMs > 0)
        return RunDummy(durationMs);

    return RunCompleter();
}
