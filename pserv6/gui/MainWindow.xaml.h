#pragma once
#include "MainWindow.g.h"
#include "action_runner.h"
#include "data_grid.h"
#include "themes.h"
#include <core/data_controller_library.h>
#include <core/refresh_coordinator.h>

namespace pserv6
{
    class SelfTest;
    class ActionTest;
}

namespace winrt::pserv6::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        void OnNavSelectionChanged(Microsoft::UI::Xaml::Controls::NavigationView const &, Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const &);
        void OnRefreshClick(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnAutoRefreshToggled(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnFilterChanged(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::Controls::TextChangedEventArgs const &);
        void OnElevate(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnCancelAction(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnRowSelectionChanged(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
        void OnContainerContentChanging(Microsoft::UI::Xaml::Controls::ListViewBase const &, Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const &);
        void OnRowContextRequested(Microsoft::UI::Xaml::UIElement const &, Microsoft::UI::Xaml::Input::ContextRequestedEventArgs const &);
        void OnRowDoubleTapped(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const &);
        winrt::fire_and_forget OnConnectRemote(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnConnectLocal(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnExit(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        void OnDocumentation(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
        winrt::fire_and_forget OnAbout(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);

        // Also used by the self-test
        bool SelectView(std::string_view name);
        void RequestRefresh(bool bAutoRefresh);
        pserv::DataController *CurrentController() const { return m_pCurrent; }
        bool IsRefreshInFlight() const { return m_pCurrent && m_coordinator && m_coordinator->IsInFlight(m_pCurrent); }
        const std::vector<pserv::DataController *> &Controllers() { return m_library.GetDataControllers(); }
        ::pserv6::DataGrid &Grid() { return m_grid; }
        /// Self-test: applies every Base16 theme and the Windows setting again; an error text or "".
        std::string CheckThemes();
        size_t ThemeCount() const { return m_themes.size(); }
        bool IsSelfTesting() const { return m_selfTest != nullptr || m_actionTest != nullptr; }
        ::pserv6::ActionRunner &Actions() { return *m_runner; }
        /// Run an action on the selection as the UI would (confirmation, parameters, execution).
        void RunAction(const pserv::DataAction *pAction, const std::vector<pserv::DataObject *> &selection);
        /// Services and Devices on another machine (empty: this machine). Other views are disabled.
        void SetRemoteMachine(const std::string &machineName);

    private:
        void ShowController(pserv::DataController *pController);
        void RunUiTask(const std::function<void()> &task);
        void OnPublished(pserv::DataController *pController);
        void UpdateStatus();
        void UpdateRefreshStatus();
        void UpdateActionBar();
        void UpdateTitle();
        void UpdateTitleBarRegions();
        const pserv::DataAction *OfferedAction(const std::string &name, const std::vector<pserv::DataObject *> &selection) const;
        void ShowActionProgress(bool bActive, double value, const std::string &text);
        bool IsOffered(const pserv::DataAction *pAction, const std::vector<pserv::DataObject *> &selection) const;
        HWND WindowHandle();
        void RestoreWindowPlacement();
        void SaveSettings();
        void AddKeyboardShortcuts();
        void CopySelection();
        void RebuildSortMenu();
        void FillColumnsMenu(Windows::Foundation::Collections::IVector<Microsoft::UI::Xaml::Controls::MenuFlyoutItemBase> const &items);
        void BuildViewMenu();
        void UpdateViewMenu();
        void ApplyTheme(const std::string &name); ///< "Light", "Dark", a Base16 theme's name; anything else: follow Windows
        void ApplyThemeToChrome();                ///< Grid brushes and title bar, for the actual theme
        void SetZoomPercent(int percent);
        void SetTableFont(const std::string &family); ///< Empty: the default font
        void ChooseTableFont();
        void SetAutoRefreshInterval(int milliseconds);
        void ShowContextMenu(pserv::DataObject *pClicked, Microsoft::UI::Xaml::UIElement const &target, std::optional<winrt::Windows::Foundation::Point> position);
        void RefreshAfterChange(pserv::DataController *pController);
    public:
        /// The controller's target or the rows it shows belong to another machine.
        bool IsRemote(const pserv::DataController *pController) const;
        uint64_t TargetGeneration() const { return m_targetGeneration; }

    private:
        static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data);

        // Lifetime: a worker task calls into the coordinator (and its controllers) after the
        // enumeration, so the worker must be joined first. Members are destroyed in reverse order:
        // m_worker, then m_coordinator, then m_library. Closed() resets them in that order too.
        pserv::DataControllerLibrary m_library;
        std::unique_ptr<pserv::RefreshCoordinator> m_coordinator;
        std::unique_ptr<pserv::RefreshWorker> m_worker;
        /// Cleared on close, so UI tasks posted by the worker afterwards do nothing.
        std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
        ::pserv6::DataGrid m_grid;
        pserv::DataController *m_pCurrent{nullptr};
        Microsoft::UI::Dispatching::DispatcherQueueTimer m_autoRefreshTimer{nullptr};
        Microsoft::UI::Dispatching::DispatcherQueueTimer m_progressHideTimer{nullptr};
        bool m_bPublishedCurrent{false}; ///< Set by OnPublished during the UI task that published.
        std::unique_ptr<::pserv6::SelfTest> m_selfTest;
        /// Runs actions and their dialogs. Destroyed before the coordinator and the library
        /// (declared after them): a running operation is waited for in its destructor.
        std::unique_ptr<::pserv6::ActionRunner> m_runner;
        HWND m_hwnd{nullptr};
        // View menu items that mirror state (owned by the menu; no handler captures them)
        std::vector<Microsoft::UI::Xaml::Controls::RadioMenuFlyoutItem> m_viewItems;
        std::vector<::pserv6::Base16Theme> m_themes{::pserv6::LoadBase16Themes()};
        std::vector<std::pair<std::string, Microsoft::UI::Xaml::Controls::RadioMenuFlyoutItem>> m_themeItems; ///< By setting value
        Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem m_autoRefreshItem{nullptr};
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem m_zoomResetItem{nullptr};
        Microsoft::UI::Xaml::Controls::MenuFlyoutSubItem m_columnsMenu{nullptr}; ///< Refilled with the Sort menu
        int m_zoomPercent{100};
        std::chrono::steady_clock::time_point m_refreshRequested{std::chrono::steady_clock::now()};
        std::string m_lastRefresh; ///< Status bar: time and duration of the last publish
        // The toolbar's action buttons (owned by the toolbar; no handler captures them), by action name
        pserv::DataController *m_pBarController{nullptr}; ///< The view they were built for
        std::vector<std::pair<std::string, Microsoft::UI::Xaml::Controls::AppBarButton>> m_barButtons;
        std::vector<std::pair<std::string, Microsoft::UI::Xaml::Controls::MenuFlyoutItem>> m_barMenuItems;
        std::vector<std::pair<Microsoft::UI::Xaml::Controls::AppBarButton, Microsoft::UI::Xaml::Controls::MenuFlyout>> m_barGroups;
        bool m_bElevated{false}; ///< This process has administrator rights
        int m_wheelDelta{0}; ///< Ctrl+wheel movement not yet turned into a zoom step
        uint64_t m_connectGeneration{0}; ///< Bumped by every connect: a superseded probe result is dropped.
        uint64_t m_targetGeneration{0};  ///< Bumped when the target machine changes (ActionRunner guard).
        std::unique_ptr<::pserv6::ActionTest> m_actionTest; ///< Destroyed before m_runner, which it drives.
    };
}

namespace winrt::pserv6::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
