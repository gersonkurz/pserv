#include "precomp.h"
#include "main_window.h"
#include "Resource.h"
#include "Config/settings.h"
#include "utils/win32_error.h"
#include "windows_api/service_manager.h"
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace pserv {

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() {
    CleanupImGui();
    CleanupDirectX();
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
    }
}

bool MainWindow::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    // Register window class
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PSERV5));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_PSERV5);
    wcex.lpszClassName = L"pserv5WindowClass";
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

    if (!RegisterClassExW(&wcex)) {
        spdlog::error("Failed to register window class");
        return false;
    }

    // Load window settings
    auto& windowSettings = config::theSettings.window;
    int width = windowSettings.width.get();
    int height = windowSettings.height.get();
    int posX = windowSettings.positionX.get();
    int posY = windowSettings.positionY.get();
    bool maximized = windowSettings.maximized.get();

    // Create window
    m_hWnd = CreateWindowW(
        L"pserv5WindowClass",
        L"pserv5",
        WS_OVERLAPPEDWINDOW,
        posX, posY,
        width, height,
        nullptr,
        nullptr,
        hInstance,
        this
    );

    if (!m_hWnd) {
        spdlog::error("Failed to create window");
        return false;
    }

    // Store 'this' pointer for WndProc access
    SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Initialize DirectX 11
    if (!InitializeDirectX()) {
        spdlog::error("Failed to initialize DirectX 11");
        return false;
    }

    // Initialize ImGui
    if (!InitializeImGui()) {
        spdlog::error("Failed to initialize ImGui");
        return false;
    }

    // Load active tab from settings
    auto& appSettings = config::theSettings.application;
    m_activeTab = appSettings.activeView.get();
    spdlog::info("Loaded active tab: {}", m_activeTab);

    spdlog::info("Main window initialized successfully");
    return true;
}

void MainWindow::Show(int nCmdShow) {
    // Override nCmdShow if we have a saved maximized state
    auto& windowSettings = config::theSettings.window;
    if (windowSettings.maximized.get()) {
        nCmdShow = SW_SHOWMAXIMIZED;
    }
    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);
}

int MainWindow::MessageLoop() {
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Render frame when idle
            Render();
        }
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    // Forward messages to ImGui first
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    // Retrieve MainWindow instance pointer
    MainWindow* pWindow = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message) {
    case WM_SIZE:
        if (pWindow && pWindow->m_pDevice && wParam != SIZE_MINIMIZED) {
            pWindow->CleanupRenderTarget();
            pWindow->m_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
            pWindow->CreateRenderTarget();
        }
        return 0;

    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId) {
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

    case WM_DESTROY:
        if (pWindow) {
            pWindow->SaveWindowState();
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

bool MainWindow::InitializeDirectX() {
    // Create swap chain description
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &m_pSwapChain,
        &m_pDevice,
        &featureLevel,
        &m_pDeviceContext
    );

    if (FAILED(hr)) {
        spdlog::error("D3D11CreateDeviceAndSwapChain failed: {:#x}", static_cast<unsigned int>(hr));
        return false;
    }

    CreateRenderTarget();
    spdlog::info("DirectX 11 initialized successfully");
    return true;
}

void MainWindow::CleanupDirectX() {
    CleanupRenderTarget();
    m_pSwapChain.Reset();
    m_pDeviceContext.Reset();
    m_pDevice.Reset();
}

void MainWindow::CreateRenderTarget() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
    m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_pRenderTargetView);
    }
}

void MainWindow::CleanupRenderTarget() {
    m_pRenderTargetView.Reset();
}

bool MainWindow::InitializeImGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    // Load custom font - try Segoe UI first (Windows standard), fall back to Arial
    wchar_t windowsDir[MAX_PATH];
    if (GetWindowsDirectoryW(windowsDir, MAX_PATH) == 0) {
        spdlog::warn("GetWindowsDirectory failed, using default font");
        io.Fonts->AddFontDefault();
    } else {
        std::wstring fontsDir = std::wstring(windowsDir) + L"\\Fonts\\";
        const wchar_t* fontFiles[] = {
            L"segoeui.ttf",  // Segoe UI (modern, clean)
            L"arial.ttf"     // Arial (fallback)
        };

        bool fontLoaded = false;
        for (const wchar_t* fontFile : fontFiles) {
            std::wstring fontPath = fontsDir + fontFile;
            // Convert to narrow string for ImGui
            int size = WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string narrowPath(size, 0);
            WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, &narrowPath[0], size, nullptr, nullptr);
            narrowPath.resize(size - 1); // Remove null terminator

            if (io.Fonts->AddFontFromFileTTF(narrowPath.c_str(), 16.0f)) {
                spdlog::info("Loaded font: {}", narrowPath);
                fontLoaded = true;
                break;
            }
        }

        if (!fontLoaded) {
            spdlog::warn("Could not load custom font, using default");
            io.Fonts->AddFontDefault();
        }
    }

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hWnd);
    ImGui_ImplDX11_Init(m_pDevice.Get(), m_pDeviceContext.Get());

    spdlog::info("ImGui initialized successfully");
    return true;
}

void MainWindow::CleanupImGui() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}

void MainWindow::Render() {
    if (!m_pDeviceContext || !m_pRenderTargetView) {
        return;
    }

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Create fullscreen window for tab bar
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("MainWindow", nullptr, window_flags);

    // Create tab bar with placeholder tabs
    if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)) {
        const char* tabs[] = {"Services", "Devices", "Processes", "Windows", "Modules", "Uninstaller"};
        static bool firstFrame = true;

        for (const char* tab : tabs) {
            // Set the initially active tab on first frame
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (firstFrame && m_activeTab == tab) {
                flags |= ImGuiTabItemFlags_SetSelected;
            }

            if (ImGui::BeginTabItem(tab, nullptr, flags)) {
                // Check if active tab changed
                if (m_activeTab != tab) {
                    m_activeTab = tab;
                    auto& appSettings = config::theSettings.application;
                    appSettings.activeView.set(m_activeTab);
                    if (m_pConfigBackend) {
                        appSettings.save(*m_pConfigBackend);
                        spdlog::debug("Active tab changed to: {}", m_activeTab);
                    }
                }

                // Placeholder content
                ImGui::Text("This is the %s view", tab);
                ImGui::Text("Content will be implemented in future milestones");

                // Test button for service enumeration (Milestone 10)
                if (std::string(tab) == "Services") {
                    ImGui::Separator();
                    if (ImGui::Button("Test: Enumerate Services")) {
                        try {
                            ServiceManager sm;
                            auto services = sm.EnumerateServices();
                            spdlog::info("Successfully enumerated {} services", services.size());
                            for (size_t i = 0; i < std::min(services.size(), size_t(5)); ++i) {
                                spdlog::info("  Service {}: {} ({})", i+1,
                                    services[i].displayName, services[i].name);
                            }
                            if (services.size() > 5) {
                                spdlog::info("  ... and {} more services", services.size() - 5);
                            }
                        } catch (const std::exception& e) {
                            spdlog::error("Failed to enumerate services: {}", e.what());
                        }
                    }
                }

                ImGui::EndTabItem();
            }
        }

        firstFrame = false;
        ImGui::EndTabBar();
    }

    ImGui::End();

    // Show ImGui demo window for testing (optional)
    if (m_bShowDemoWindow) {
        ImGui::ShowDemoWindow(&m_bShowDemoWindow);
    }

    // Rendering
    ImGui::Render();

    // Clear to cornflower blue (classic DirectX default)
    const float clearColor[4] = { 0.392f, 0.584f, 0.929f, 1.0f };
    m_pDeviceContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), nullptr);
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), clearColor);

    // Render ImGui
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present
    m_pSwapChain->Present(1, 0); // VSync enabled
}

void MainWindow::SaveWindowState() {
    if (!m_hWnd || !m_pConfigBackend) {
        spdlog::warn("Cannot save window state: hWnd={}, pConfigBackend={}",
            (void*)m_hWnd, (void*)m_pConfigBackend);
        return;
    }

    auto& windowSettings = config::theSettings.window;

    // Check if maximized
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(WINDOWPLACEMENT);
    if (!GetWindowPlacement(m_hWnd, &wp)) {
        LogWin32Error("GetWindowPlacement");
        return;
    }

    windowSettings.maximized.set(wp.showCmd == SW_SHOWMAXIMIZED);

    // Get normal (non-maximized) position
    RECT& rc = wp.rcNormalPosition;
    windowSettings.positionX.set(rc.left);
    windowSettings.positionY.set(rc.top);
    windowSettings.width.set(rc.right - rc.left);
    windowSettings.height.set(rc.bottom - rc.top);

    // Save to config backend
    windowSettings.save(*m_pConfigBackend);

    spdlog::info("Window state saved: {}x{} at ({}, {}), maximized={}",
        windowSettings.width.get(), windowSettings.height.get(),
        windowSettings.positionX.get(), windowSettings.positionY.get(),
        windowSettings.maximized.get());
}

} // namespace pserv
