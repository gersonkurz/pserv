#pragma once
#include "MainWindow.g.h"
#include "data_grid.h"
#include <core/data_controller_library.h>
#include <core/refresh_coordinator.h>

namespace pserv6
{
    class SelfTest;
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
        void OnContainerContentChanging(Microsoft::UI::Xaml::Controls::ListViewBase const &, Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const &);

        // Also used by the self-test
        bool SelectView(std::string_view name);
        void RequestRefresh(bool bAutoRefresh);
        pserv::DataController *CurrentController() const { return m_pCurrent; }
        bool IsRefreshInFlight() const { return m_pCurrent && m_coordinator && m_coordinator->IsInFlight(m_pCurrent); }
        const std::vector<pserv::DataController *> &Controllers() { return m_library.GetDataControllers(); }
        ::pserv6::DataGrid &Grid() { return m_grid; }

    private:
        void ShowController(pserv::DataController *pController);
        void RunUiTask(const std::function<void()> &task);
        void OnPublished(pserv::DataController *pController);
        void UpdateStatus();
        HWND WindowHandle();
        void RestoreWindowPlacement();
        void SaveSettings();
        void AddKeyboardShortcuts();

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
        bool m_bPublishedCurrent{false}; ///< Set by OnPublished during the UI task that published.
        std::unique_ptr<::pserv6::SelfTest> m_selfTest;
    };
}

namespace winrt::pserv6::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
