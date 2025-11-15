#include "precomp.h"
#include "main_window.h"
#include "Resource.h"
#include <dxgi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace pserv {

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() {
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

    // Create window
    m_hWnd = CreateWindowW(
        L"pserv5WindowClass",
        L"pserv5",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
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

    spdlog::info("Main window initialized successfully");
    return true;
}

void MainWindow::Show(int nCmdShow) {
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

void MainWindow::Render() {
    if (!m_pDeviceContext || !m_pRenderTargetView) {
        return;
    }

    // Clear to cornflower blue (classic DirectX default)
    const float clearColor[4] = { 0.392f, 0.584f, 0.929f, 1.0f };
    m_pDeviceContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), nullptr);
    m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), clearColor);

    // Present
    m_pSwapChain->Present(1, 0); // VSync enabled
}

} // namespace pserv
