#pragma once

#include <core/data_action.h>
#include <core/action_host.h>
#include <core/data_controller_library.h>
#include <core/data_action_dispatch_context.h>
#include <theme/base16_theme.h>
#include <dialogs/theme_selection_dialog.h>
#include <dialogs/data_properties_dialog.h>

struct ImGuiTable; // Forward declaration

namespace pserv
{
    namespace config
    {
        class ConfigBackend;
    }

    /// The ImGui frontend's main window. It is also this frontend's IActionHost: action messages go
    /// to its error dialog and "Properties..." opens its properties dialog.
    class MainWindow final : public IActionHost
    {
    public:
        MainWindow();
        ~MainWindow() override;

        // IActionHost
        void ShowMessage(ActionMessageKind kind, std::string_view title, std::string_view text) override;
        void ShowProperties(DataActionDispatchContext &ctx) override;

        bool Initialize(HINSTANCE hInstance);
        void Show(int nCmdShow);
        int MessageLoop();
        void SetConfigBackend(config::ConfigBackend *pBackend)
        {
            m_pConfigBackend = pBackend;
        }
        void SetAppDataPath(const std::filesystem::path &appDataPath)
        {
            m_imguiIniPath = (appDataPath / "imgui.ini").string();
        }

        // Internal methods (used by Render())
        void RenderSplashFrame();
        void PreloadActiveController();

    private:
        enum class AppState
        {
            Splash,      // Showing splash screen
            Loading,     // Loading data in background
            Ready        // Normal operation
        };

        AppState m_appState{AppState::Splash};
        std::thread m_loadThread;
        std::atomic<bool> m_loadingComplete{false};
        static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

        bool InitializeDirectX();
        void CleanupDirectX();
        bool InitializeImGui();
        void CleanupImGui();
        void ApplyTheme(const std::string &name);
        std::wstring GetThemesDirectory() const;
        void RebuildFontAtlas(float fontSize);
        void CreateRenderTarget();
        void CleanupRenderTarget();
        void Render();
        void RenderDataController(DataController *controller);

        // Splash screen (internal)
        bool LoadSplashImage();

        HWND m_hWnd{nullptr};
        HINSTANCE m_hInstance{nullptr};
        config::ConfigBackend *m_pConfigBackend{nullptr};
        std::string m_imguiIniPath;

        // DirectX 11 resources
        Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pDeviceContext;
        Microsoft::WRL::ComPtr<IDXGISwapChain> m_pSwapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;
        DataControllerLibrary m_Controllers;

        // Splash screen
        ID3D11ShaderResourceView *m_pSplashTexture{nullptr};
        int m_splashWidth{0};
        int m_splashHeight{0};

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

        // Theming: Base16 schemes loaded from themes/ beside the exe, plus the picker.
        theme::ThemeSet m_themeSet;
        std::unique_ptr<ThemeSelectionDialog> m_pThemeDialog;

        // Auto-refresh state
        std::chrono::steady_clock::time_point m_lastAutoRefreshTime;

        // Remote machine connection dialog state
        bool m_bShowRemoteMachineDialog{false};

        // Error dialog state
        bool m_bShowErrorDialog{false};
        std::string m_errorDialogMessage;

        // Properties dialogs, one per controller (a view keeps its dialog while another view is shown).
        std::map<DataController *, std::unique_ptr<DataPropertiesDialog>> m_propertiesDialogs;

        // An action waiting for its text parameters (ImGui modal), collected before dispatch.
        struct PendingAction
        {
            const DataAction *pAction{nullptr};
            DataController *pController{nullptr}; ///< The view the action was requested in (views can switch while pending).
            std::vector<DataObject *> selection; ///< Retained while pending.
            std::vector<ActionParameter> parameters;
            std::map<std::string, std::string> values;
            std::vector<std::array<char, 4096>> buffers; ///< ImGui edit buffers, one per Text parameter.
            bool bOpenRequested{false};
        } m_pendingAction;

        // Action dispatch: confirmation and parameters are collected here, before Execute().
        void RequestAction(const DataAction *pAction, const std::vector<DataObject *> &selection);
        void ExecuteAction(DataController *pController, const DataAction *pAction, const std::vector<DataObject *> &selection,
            std::map<std::string, std::string> values);

        // Refreshes requested while a table may be rendering (an action run from a row's context
        // menu, a nested modal loop dispatching WM_ASYNC_OPERATION_COMPLETE) are deferred to the
        // start of the next frame: a refresh can release objects the table loop still points at.
        std::set<DataController *> m_deferredRefreshes;
        void RunDeferredRefreshes();
        void RenderPendingActionDialog();
        void ClearPendingAction();
        void RenderPropertiesDialogs();

        // Helper methods
        bool ShouldAutoRefresh() const;
        void SaveWindowState();
        void SaveCurrentTableState(bool force = false);
        void RenderProgressDialog();
        void RenderRemoteMachineDialog();
        void RenderErrorDialog();
        void RenderTitleBar();
        void RenderMenuBar();
        bool IsWindowMaximized() const;
    };

} // namespace pserv
