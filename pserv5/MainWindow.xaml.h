#pragma once
#include "MainWindow.g.h"
#include <core/data_action.h>
#include <core/data_controller_library.h>
#include <core/data_action_dispatch_context.h>
#include <theme/base16_theme.h>
#include <config/config_backend.h>

namespace winrt::pserv5::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        void SetConfigBackend(pserv::config::ConfigBackend *pBackend)
        {
            m_pConfigBackend = pBackend;
        }

        void BringToFront();

        // XAML Event Handlers
        void OnRootKeyDown(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const &e);
        void OnRefreshClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnPropertiesClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnConnectLocalClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnConnectRemoteClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnExitClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnAutoRefreshToggleClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnAutoRefreshIntervalClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnIncreaseFontSizeClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnDecreaseFontSizeClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnAboutClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnDocumentationClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnFilterTextChanged(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::Controls::TextChangedEventArgs const &e);
        void OnRootPointerWheelChanged(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const &e);
        void OnNavSelectionChanged(winrt::Microsoft::UI::Xaml::Controls::NavigationView const &sender, winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const &args);
        void OnAsyncCancelClick(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::RoutedEventArgs const &e);
        void OnContainerContentChanging(winrt::Microsoft::UI::Xaml::Controls::ListViewBase const &sender, winrt::Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const &args);
        void OnListSelectionChanged(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &e);
        void OnListDoubleTapped(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const &e);
        void OnListRightTapped(winrt::Windows::Foundation::IInspectable const &sender, winrt::Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const &e);

    private:
        void InitializeWindow();
        void PopulateNavigationAndMenus();
        void PopulateThemesMenu();
        void UpdateThemesMenuChecks();
        void ApplyTheme(const std::string &themeName);
        void ChangeFontSize(double delta);
        std::wstring GetThemesDirectory() const;

        void SwitchToController(pserv::DataController *controller);
        void BuildColumnHeaders();
        void LoadColumnStateForController(pserv::DataController *controller);
        void SaveCurrentTableState();
        void SaveWindowState();

        void RebuildFilteredList(bool preserveSelection = true, bool forceReset = false);
        void RefreshVisibleRowsInPlace();
        void UpdateRowVisuals(winrt::Microsoft::UI::Xaml::Controls::Grid const &rowGrid, pserv::DataObject *dataObject);
        void SyncListScrollViewer();
        void UpdateStatusBar();
        void UpdateQuickActionButtons();
        void UpdateRemoteModeUI();

        void ExecuteAction(const pserv::DataAction *action);
        void HandlePostActionContext();
        winrt::Windows::Foundation::IAsyncAction ShowPropertiesDialogAsync();
        winrt::Windows::Foundation::IAsyncAction ShowRemoteMachineDialogAsync();
        winrt::Windows::Foundation::IAsyncAction ShowAddEnvironmentVariableDialogAsync(int scope);
        winrt::Windows::Foundation::IAsyncAction ShowErrorDialogAsync(std::string message);
        winrt::Windows::Foundation::IAsyncAction ShowAboutDialogAsync();

        void OnTimerTick();
        bool ShouldAutoRefresh() const;

        static LRESULT CALLBACK SubclassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
        void OnAsyncOperationComplete();

    private:
        HWND m_hWnd{nullptr};
        winrt::Microsoft::UI::Windowing::AppWindow m_appWindow{nullptr};
        pserv::config::ConfigBackend *m_pConfigBackend{nullptr};

        pserv::DataControllerLibrary m_controllers;
        pserv::DataController *m_pCurrentController{nullptr};
        pserv::DataActionDispatchContext m_dispatchContext;

        std::vector<pserv::DataObject *> m_filteredObjects;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_listItems{nullptr};
        std::vector<const pserv::DataAction *> m_lastToolbarActions;
        std::vector<std::pair<std::string, winrt::Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem>> m_themeMenuItems;
        std::vector<double> m_columnWidths;
        uint32_t m_currentColumnSchemaId{0};
        double m_fontSize{13.0};

        pserv::theme::ThemeSet m_themeSet;
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush m_normalTextBrush{nullptr};
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush m_highlightTextBrush{nullptr};
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush m_disabledTextBrush{nullptr};
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush m_altRowBrush{nullptr};
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush m_transparentBrush{nullptr};
        winrt::Microsoft::UI::Xaml::Media::SolidColorBrush m_destructiveBrush{nullptr};

        winrt::Microsoft::UI::Xaml::Controls::ScrollViewer m_listScrollViewer{nullptr};
        winrt::Microsoft::UI::Xaml::DispatcherTimer m_periodicTimer{nullptr};
        std::chrono::steady_clock::time_point m_lastAutoRefreshTime;

        bool m_bIsSwitchingController{false};
        bool m_bIsUpdatingSelection{false};
        bool m_bDialogOpen{false};
        bool m_bTableDirty{false};
    };
}

namespace winrt::pserv5::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
