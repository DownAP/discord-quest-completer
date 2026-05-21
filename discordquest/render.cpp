#include "render.h"
#include "ui.h"
#include "resource.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    ID3D11Device*           g_device        = nullptr;
    ID3D11DeviceContext*    g_context       = nullptr;
    IDXGISwapChain*         g_swapChain     = nullptr;
    ID3D11RenderTargetView* g_renderTarget  = nullptr;
    bool                    g_occluded      = false;
    UINT                    g_resizeW       = 0;
    UINT                    g_resizeH       = 0;

    void CreateRenderTarget()
    {
        ID3D11Texture2D* backBuffer = nullptr;
        g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (backBuffer)
        {
            g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTarget);
            backBuffer->Release();
        }
    }

    void CleanupRenderTarget()
    {
        if (g_renderTarget) { g_renderTarget->Release(); g_renderTarget = nullptr; }
    }

    bool CreateDeviceD3D(HWND hWnd)
    {
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount                        = 2;
        sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator   = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow                       = hWnd;
        sd.SampleDesc.Count                   = 1;
        sd.Windowed                           = TRUE;
        sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL chosen;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
            D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &chosen, &g_context);
        if (hr == DXGI_ERROR_UNSUPPORTED)
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &chosen, &g_context);
        if (FAILED(hr))
            return false;

        CreateRenderTarget();
        return true;
    }

    void CleanupDeviceD3D()
    {
        CleanupRenderTarget();
        if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
        if (g_context)   { g_context->Release();   g_context   = nullptr; }
        if (g_device)    { g_device->Release();    g_device    = nullptr; }
    }

    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        switch (msg)
        {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
            {
                g_resizeW = (UINT)LOWORD(lParam);
                g_resizeH = (UINT)HIWORD(lParam);
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        }
        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

int RunImGuiWindow(const RenderOptions& opts, const std::function<bool()>& frame)
{
    ImGui_ImplWin32_EnableDpiAwareness();

    HINSTANCE hInst = ::GetModuleHandleW(nullptr);
    HICON icon = ::LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = icon;
    wc.hIconSm       = icon;
    wc.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DiscordQuestWindow";
    ::RegisterClassExW(&wc);

    const DWORD style = opts.resizable
        ? WS_OVERLAPPEDWINDOW
        : (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

    RECT rc = { 0, 0, opts.width, opts.height };
    ::AdjustWindowRect(&rc, style, FALSE);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int x = (::GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    const int y = (::GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    HWND hWnd = ::CreateWindowW(wc.lpszClassName, opts.title, style,
                                x, y, w, h, nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hWnd))
    {
        CleanupDeviceD3D();
        ::DestroyWindow(hWnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hWnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    SetupImGuiStyle();

    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_occluded && g_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_occluded = false;

        if (g_resizeW != 0 && g_resizeH != 0)
        {
            CleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, g_resizeW, g_resizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeW = g_resizeH = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const bool keepRunning = frame();

        ImGui::Render();
        const float clear[4] = { 0.067f, 0.070f, 0.078f, 1.0f };
        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        g_context->ClearRenderTargetView(g_renderTarget, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_swapChain->Present(1, 0);
        g_occluded = (hr == DXGI_STATUS_OCCLUDED);

        if (!keepRunning)
            done = true;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hWnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
