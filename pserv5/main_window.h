#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

namespace pserv {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Initialize(HINSTANCE hInstance);
    void Show(int nCmdShow);
    int MessageLoop();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    bool InitializeDirectX();
    void CleanupDirectX();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void Render();

    HWND m_hWnd{nullptr};
    HINSTANCE m_hInstance{nullptr};

    // DirectX 11 resources
    Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pDeviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_pSwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;
};

} // namespace pserv
