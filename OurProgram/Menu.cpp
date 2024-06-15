#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <string>
#include "Config.h"

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImFont* regular_font;
ImFont* bold_font;
ImFont* italic_font;
ImFont* huge_font;

// Define the display names and corresponding values separately
const char* keyNames[] = {
    "Left Mouse Button",
    "Right Mouse Button",
    "Middle Mouse Button",
    "Side1 Mouse Button",
    "Side2 Mouse Button",
};
int keyValues[] = {
    0x01, // Left Mouse Button
    0x02, // Right Mouse Button
    0x04, // Middle Mouse Button
    0x05, // Side1 Mouse Button
    0x06, // Side2 Mouse Button
};
constexpr int keyCount = sizeof(keyNames) / sizeof(keyNames[0]);

int FindCurrentSelectionIndex(const int value, const int* keyValues, const int keyCount) {
    for (int i = 0; i < keyCount; ++i) {
        if (value == keyValues[i]) {
            return i;
        }
    }
    return 0; // Default to the first item if not found
}

// Main code
void Render(Config& config)
{
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGuiExample", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName, L"ImGuiExample", WS_POPUP, 0, 0, width, height, nullptr, nullptr, wc.hInstance, nullptr);

    // Set Transparency, WS_EX_LAYERED is required for transparency
    SetLayeredWindowAttributes(hwnd, 0, RGB(0, 0, 0), LWA_COLORKEY);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup fonts
    regular_font = io.Fonts->AddFontFromFileTTF("regular.otf", 12.0f);
    bold_font = io.Fonts->AddFontFromFileTTF("bold.otf", 12.0f);
    italic_font = io.Fonts->AddFontFromFileTTF("italic.otf", 12.0f);
    huge_font = io.Fonts->AddFontFromFileTTF("bold.otf", 20.0f);

    io.FontDefault = bold_font;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Main loop
    bool running = true;
    while (running)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                running = false;
        }

        if (!running)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Foreground Drawlist
        ImDrawList* fg_draw = ImGui::GetForegroundDrawList();

        // Performance
        std::string PerformanceString = "Render FPS: " + std::to_string(static_cast<int>(ImGui::GetIO().Framerate));
        fg_draw->AddText(ImVec2(10, 50), IM_COL32(255, 255, 255, 255), PerformanceString.c_str());

        // Settings Window, Fixed Width
        ImGui::SetNextWindowSize(ImVec2(400, 430), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(width - 410, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings");

        // Example Settings
        ImGui::PushFont(huge_font);
        ImGui::Text("Example Settings");
        ImGui::PopFont();
        ImGui::Text("Example Bool:");
        ImGui::SameLine();
        if (ImGui::Checkbox("##Example Bool:", &config.exampleBool)) {
            SaveConfig(config);
        }

        ImGui::Text("Example Float:");
        if (ImGui::SliderFloat("##Example Float:", &config.exampleFloat, 1.0f, 50.0f, "% .1f")) {
            SaveConfig(config);
        }

        ImGui::Text("Example Int:");
        if (ImGui::SliderInt("##Example Int:", &config.exampleInt, 1, 100)) {
            SaveConfig(config);
        }

        // For ExampleKey
        static int exampleKeyIndex = FindCurrentSelectionIndex(config.exampleInt, keyValues, keyCount);
        ImGui::Text("Example Key:");
        if (ImGui::Combo("##ExampleKey", &exampleKeyIndex, keyNames, keyCount)) {
            config.exampleInt = keyValues[exampleKeyIndex];
            SaveConfig(config);
        }

        // Exit Button
        if (ImGui::Button("Exit"))
        {
            running = false;
            PostQuitMessage(0);
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Adjust the window style to allow interactions
        DWORD dwExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        {
            SetWindowLong(hwnd, GWL_EXSTYLE, dwExStyle & ~WS_EX_TRANSPARENT);
        }
        else
        {
            SetWindowLong(hwnd, GWL_EXSTYLE, dwExStyle | WS_EX_TRANSPARENT);
        }

        // Ensure the window is always on top and focused
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (GetForegroundWindow() != hwnd && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        {
            SetForegroundWindow(hwnd);
        }

        g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
