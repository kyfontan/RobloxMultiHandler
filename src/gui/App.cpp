/*
 * Win32 host window + D3D11 swapchain + Dear ImGui main loop.
 */
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <objbase.h>

#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_win32.h"
#include "../third_party/imgui/backends/imgui_impl_dx11.h"

#include "Tabs.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define APP_W 720
#define APP_H 620

extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

struct Gfx {
    ID3D11Device          *device  = nullptr;
    ID3D11DeviceContext   *ctx     = nullptr;
    IDXGISwapChain        *swap    = nullptr;
    ID3D11RenderTargetView *rtv    = nullptr;
};

static Gfx                          g_gfx;
static rh::gui::AppState           *g_state = nullptr;
static bool                         g_quit  = false;

bool create_render_target(Gfx &g) {
    ID3D11Texture2D *back = nullptr;
    if (g.swap->GetBuffer(0, IID_PPV_ARGS(&back)) != S_OK || !back) return false;
    HRESULT hr = g.device->CreateRenderTargetView(back, NULL, &g.rtv);
    back->Release();
    return hr == S_OK;
}

void release_render_target(Gfx &g) {
    if (g.rtv) { g.rtv->Release(); g.rtv = nullptr; }
}

bool init_gfx(Gfx &g, HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width  = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL featLevels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL chosen;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
        featLevels, 2, D3D11_SDK_VERSION, &sd,
        &g.swap, &g.device, &chosen, &g.ctx);
    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags,
            featLevels, 2, D3D11_SDK_VERSION, &sd,
            &g.swap, &g.device, &chosen, &g.ctx);
    }
    if (hr != S_OK) return false;
    return create_render_target(g);
}

void destroy_gfx(Gfx &g) {
    release_render_target(g);
    if (g.swap)   { g.swap->Release();   g.swap   = nullptr; }
    if (g.ctx)    { g.ctx->Release();    g.ctx    = nullptr; }
    if (g.device) { g.device->Release(); g.device = nullptr; }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;

    switch (msg) {
        case WM_SIZE:
            if (g_gfx.device && wp != SIZE_MINIMIZED) {
                release_render_target(g_gfx);
                g_gfx.swap->ResizeBuffers(0, (UINT)LOWORD(lp), (UINT)HIWORD(lp),
                                          DXGI_FORMAT_UNKNOWN, 0);
                create_render_target(g_gfx);
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xfff0) == SC_KEYMENU) return 0; /* swallow Alt menu */
            break;
        case WM_CLOSE:
            g_quit = true;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void apply_style() {
    ImGui::StyleColorsDark();
    ImGuiStyle &st = ImGui::GetStyle();
    st.WindowRounding   = 4.0f;
    st.FrameRounding    = 3.0f;
    st.GrabRounding     = 3.0f;
    st.TabRounding      = 3.0f;
    st.ScrollbarRounding= 3.0f;
    st.WindowPadding    = ImVec2(10, 8);
    st.FramePadding     = ImVec2(8, 4);
    st.ItemSpacing      = ImVec2(8, 6);
    st.SeparatorTextAlign  = ImVec2(0.0f, 0.5f);
    st.SeparatorTextPadding= ImVec2(20, 3);
}

void try_load_font(ImGuiIO &io) {
    char path[MAX_PATH];
    UINT n = GetWindowsDirectoryA(path, sizeof(path));
    if (n == 0 || n >= sizeof(path)) return;
    char full[MAX_PATH];
    snprintf(full, sizeof(full), "%s\\Fonts\\segoeui.ttf", path);
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) return;
    io.Fonts->AddFontFromFileTTF(full, 16.0f);
}

} /* anon */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    /* Register host window class. */
    WNDCLASSEXA wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconA(hInst, MAKEINTRESOURCEA(101));
    wc.hIconSm       = wc.hIcon;
    wc.lpszClassName = "RhAppHost";
    RegisterClassExA(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc = { 0, 0, APP_W, APP_H };
    AdjustWindowRectEx(&rc, style, FALSE, 0);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;

    HWND hwnd = CreateWindowExA(0, "RhAppHost", "Roblox Handler",
        style, sx, sy, ww, wh, NULL, NULL, hInst, NULL);
    if (!hwnd) { CoUninitialize(); return 1; }

    if (!init_gfx(g_gfx, hwnd)) {
        DestroyWindow(hwnd);
        CoUninitialize();
        return 2;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;        /* don't write imgui.ini next to the exe */
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    try_load_font(io);
    apply_style();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_gfx.device, g_gfx.ctx);

    rh::gui::AppState state;
    g_state = &state;
    state.host_hwnd = hwnd;
    state.device    = g_gfx.device;
    rh::gui::app_state_init(state);

    const ImVec4 clear = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
    MSG msg;
    while (!g_quit) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_quit = true;
        }
        if (g_quit) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        rh::gui::draw_root(state);

        ImGui::Render();
        const float c[4] = { clear.x, clear.y, clear.z, clear.w };
        g_gfx.ctx->OMSetRenderTargets(1, &g_gfx.rtv, NULL);
        g_gfx.ctx->ClearRenderTargetView(g_gfx.rtv, c);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_gfx.swap->Present(1, 0);   /* vsync */
    }

    /* Shutdown */
    rh::gui::persist_settings(state);
    rh::gui::app_state_destroy(state);

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    destroy_gfx(g_gfx);
    DestroyWindow(hwnd);
    UnregisterClassA("RhAppHost", hInst);
    CoUninitialize();
    return 0;
}
