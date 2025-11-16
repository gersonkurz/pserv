#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

struct ImGuiTable;  // Forward declaration

namespace pserv::config {
    class ConfigBackend;
}

namespace pserv {

class ServiceInfo;  // Forward declaration
class AsyncOperation;  // Forward declaration
class ServicesDataController;  // Forward declaration
class ServicePropertiesDialog;  // Forward declaration

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Initialize(HINSTANCE hInstance);
    void Show(int nCmdShow);
    int MessageLoop();
    void SetConfigBackend(config::ConfigBackend* pBackend) { m_pConfigBackend = pBackend; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    bool InitializeDirectX();
    void CleanupDirectX();
    bool InitializeImGui();
    void CleanupImGui();
    void RebuildFontAtlas(float fontSize);
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void Render();

    HWND m_hWnd{nullptr};
    HINSTANCE m_hInstance{nullptr};
    config::ConfigBackend* m_pConfigBackend{nullptr};

    // DirectX 11 resources
    Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pDeviceContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_pSwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;

    // ImGui state
    std::string m_activeTab{"Services"};
    std::vector<ServiceInfo*> m_services;  // Cached service list (legacy, will be removed)
    AsyncOperation* m_pAsyncOp{nullptr};  // Current async operation
    bool m_bShowProgressDialog{false};
    ServicesDataController* m_pServicesController{nullptr};  // Services controller
    ImGuiTable* m_pServicesTable{nullptr};  // Cached pointer to services table
    char m_filterText[256]{};  // Filter text for services view
    std::vector<const ServiceInfo*> m_selectedServices;  // Selected services for multi-select
    const ServiceInfo* m_lastClickedService{nullptr};  // For shift-click range selection
    ServicePropertiesDialog* m_pPropertiesDialog{nullptr};  // Service properties dialog
    float m_pendingFontSize{0.0f};  // Pending font size change (0 = no change pending)
    bool m_bWindowFocused{true};  // Track window focus state for title bar styling
    COLORREF m_accentColor{0};  // Windows accent color

    // Helper methods
    void SaveWindowState();
    void SaveServicesTableState(bool force = false);
    void ClearServices();
    void RenderProgressDialog();
    void RenderTitleBar();
    void RenderMenuBar();
    bool IsWindowMaximized() const;
};

} // namespace pserv
