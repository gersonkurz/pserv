#pragma once

#include <core/data_controller_library.h>
#include <core/data_action_dispatch_context.h>

struct ImGuiTable; // Forward declaration

namespace pserv
{
    namespace config
    {
        class ConfigBackend;
    }

    class MainWindow
    {
    public:
        MainWindow();
        ~MainWindow();

        bool Initialize(HINSTANCE hInstance);
        void Show(int nCmdShow);
        int MessageLoop();
        void SetConfigBackend(config::ConfigBackend *pBackend)
        {
            m_pConfigBackend = pBackend;
        }

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
        void RenderDataController(DataController *controller);

        HWND m_hWnd{nullptr};
        HINSTANCE m_hInstance{nullptr};
        config::ConfigBackend *m_pConfigBackend{nullptr};

        // DirectX 11 resources
        Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pDeviceContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain> m_pSwapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;
        DataControllerLibrary m_Controllers;

        // ImGui state
        std::string m_activeTab;
        DataController *m_pCurrentController{nullptr};
        std::string m_pendingTabSwitch; // Tab to switch to (empty = no pending switch)
        DataActionDispatchContext m_dispatchContext;
        ImGuiTable *m_pCurrentTable{nullptr}; // Cached pointer to services table
        char m_filterText[256]{};             // Filter text for services view

        const DataObject *m_lastClickedObject{nullptr}; // For shift-click range selection
        float m_pendingFontSize{0.0f};                  // Pending font size change (0 = no change pending)
        bool m_bWindowFocused{true};                    // Track window focus state for title bar styling
        COLORREF m_accentColor{0};                      // Windows accent color

        // Helper methods
        void SaveWindowState();
        void SaveCurrentTableState(bool force = false);
        void RenderProgressDialog();
        void RenderTitleBar();
        void RenderMenuBar();
        bool IsWindowMaximized() const;
    };

} // namespace pserv
