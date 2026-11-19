#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "self_test.h"
#include <config/settings.h>
#include <core/data_controller.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt::pserv6::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar(false);
        m_grid.Attach(RowList(), HeaderGrid());

        // Background refresh (plan D5): one worker thread for every controller; completions come
        // back to this UI thread through the DispatcherQueue.
        m_worker = std::make_unique<pserv::RefreshWorker>();
        auto dispatcher = DispatcherQueue();
        std::weak_ptr<bool> alive = m_alive;
        m_coordinator = std::make_unique<pserv::RefreshCoordinator>(
            [worker = m_worker.get()](auto task) { worker->Post(std::move(task)); },
            [this, dispatcher, alive](auto task)
            {
                // Runs on the worker; 'this' is only touched on the UI thread while alive
                dispatcher.TryEnqueue([this, alive, task = std::move(task)]
                    {
                        if (auto a = alive.lock(); a && *a)
                            RunUiTask(task);
                    });
            });
        m_coordinator->m_onPublished = [this](pserv::DataController *c) { OnPublished(c); };
        m_coordinator->m_onFailed = [this](pserv::DataController *c)
        {
            if (c == m_pCurrent)
                StatusText().Text(to_hstring(std::format("Refreshing {} failed (see log)", c->GetControllerName())));
        };

        for (auto *controller : m_library.GetDataControllers())
        {
            NavigationViewItem item;
            item.Content(box_value(to_hstring(std::string{controller->GetControllerName()})));
            item.Tag(box_value(to_hstring(std::string{controller->GetControllerName()})));
            Nav().MenuItems().Append(item);
        }

        auto &settings = pserv::config::theSettings;
        AutoRefreshSwitch().IsOn(settings.autoRefresh.enabled.get());
        m_autoRefreshTimer = DispatcherQueue().CreateTimer();
        m_autoRefreshTimer.Interval(std::chrono::milliseconds(std::clamp(settings.autoRefresh.intervalMs.get(), 1000, 10000)));
        m_autoRefreshTimer.IsRepeating(true);
        m_autoRefreshTimer.Tick([this](auto &&, auto &&) { RequestRefresh(true); });
        if (AutoRefreshSwitch().IsOn())
            m_autoRefreshTimer.Start();

        AddKeyboardShortcuts();
        RestoreWindowPlacement();

        Closed([this](auto &&, auto &&)
            {
                if (!m_selfTest) // a self-test run leaves the user's settings alone
                    SaveSettings();
                *m_alive = false;
                if (m_autoRefreshTimer)
                    m_autoRefreshTimer.Stop();
                // Join the worker first: a running enumeration finishes and posts its completion to
                // the coordinator (which queues a UI task that m_alive turns into a no-op).
                m_worker.reset();
                m_coordinator.reset();
            });

        m_selfTest = ::pserv6::SelfTest::FromCommandLine(*this);
        if (!m_selfTest && !SelectView(settings.application.activeView.get()))
            Nav().SelectedItem(Nav().MenuItems().GetAt(0));
    }

    MainWindow::~MainWindow() = default;

    bool MainWindow::SelectView(std::string_view name)
    {
        for (auto const &entry : Nav().MenuItems())
        {
            auto item = entry.as<NavigationViewItem>();
            if (to_string(unbox_value<hstring>(item.Tag())) == name)
            {
                Nav().SelectedItem(item); // -> OnNavSelectionChanged
                return true;
            }
        }
        return false;
    }

    void MainWindow::OnNavSelectionChanged(NavigationView const &, NavigationViewSelectionChangedEventArgs const &args)
    {
        auto item = args.SelectedItem().try_as<NavigationViewItem>();
        if (!item)
            return;
        const std::string name = to_string(unbox_value<hstring>(item.Tag()));
        for (auto *controller : m_library.GetDataControllers())
        {
            if (controller->GetControllerName() == name)
            {
                ShowController(controller);
                return;
            }
        }
    }

    void MainWindow::ShowController(pserv::DataController *pController)
    {
        if (m_pCurrent && m_pCurrent != pController)
        {
            // A refresh of the view we leave must not publish over what a later visit shows
            // (plan section 5.1, view switch).
            m_pCurrent->InvalidatePendingRefreshes();
        }
        m_pCurrent = pController;
        m_grid.SetController(pController);
        Nav().Header(box_value(to_hstring(std::string{pController->GetControllerName()})));
        RequestRefresh(false);
        UpdateStatus();
    }

    void MainWindow::RequestRefresh(bool bAutoRefresh)
    {
        if (m_pCurrent && m_coordinator && (!bAutoRefresh || m_pCurrent->SupportsAutoRefresh()))
            m_coordinator->Request(m_pCurrent, bAutoRefresh);
    }

    void MainWindow::RunUiTask(const std::function<void()> &task)
    {
        // The task is the coordinator's publish step; timing the whole of it gives the UI-thread
        // cost per refresh (Publish + grid diff + realized update, plan section 7).
        m_bPublishedCurrent = false;
        const auto start = std::chrono::steady_clock::now();
        task();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (m_bPublishedCurrent && m_selfTest)
            m_selfTest->OnPublished(ms);
    }

    void MainWindow::OnPublished(pserv::DataController *pController)
    {
        if (pController != m_pCurrent)
            return;
        m_grid.Refresh();
        UpdateStatus();
        m_bPublishedCurrent = true;
    }

    void MainWindow::UpdateStatus()
    {
        if (!m_pCurrent)
            return;
        const size_t total = m_pCurrent->GetDataObjects().GetSize();
        std::string text = m_pCurrent->IsLoaded()
                               ? std::format("{} {}s", m_grid.RowCount(), m_pCurrent->GetItemName())
                               : std::string{"Loading..."};
        if (m_grid.RowCount() != total && m_pCurrent->IsLoaded())
            text += std::format(" (of {})", total);
        StatusText().Text(to_hstring(text));
    }

    void MainWindow::OnRefreshClick(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        RequestRefresh(false);
    }

    void MainWindow::OnAutoRefreshToggled(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        if (!m_autoRefreshTimer)
            return;
        if (AutoRefreshSwitch().IsOn())
            m_autoRefreshTimer.Start();
        else
            m_autoRefreshTimer.Stop();
        pserv::config::theSettings.autoRefresh.enabled.set(AutoRefreshSwitch().IsOn());
    }

    void MainWindow::OnFilterChanged(Windows::Foundation::IInspectable const &, TextChangedEventArgs const &)
    {
        m_grid.SetFilter(pserv::utils::ToLower(to_string(FilterBox().Text())));
        UpdateStatus();
    }

    void MainWindow::OnContainerContentChanging(ListViewBase const &, ContainerContentChangingEventArgs const &args)
    {
        m_grid.OnContainerContentChanging(args);
    }

    void MainWindow::AddKeyboardShortcuts()
    {
        auto root = Content().as<UIElement>();
        auto add = [&root](Windows::System::VirtualKey key, Windows::System::VirtualKeyModifiers modifiers, auto handler)
        {
            Input::KeyboardAccelerator accelerator;
            accelerator.Key(key);
            accelerator.Modifiers(modifiers);
            accelerator.Invoked([handler](auto &&, Input::KeyboardAcceleratorInvokedEventArgs const &args)
                {
                    handler();
                    args.Handled(true);
                });
            root.KeyboardAccelerators().Append(accelerator);
        };
        using Windows::System::VirtualKey;
        using Windows::System::VirtualKeyModifiers;
        add(VirtualKey::F5, VirtualKeyModifiers::None, [this] { RequestRefresh(false); });
        add(VirtualKey::F, VirtualKeyModifiers::Control, [this] { FilterBox().Focus(FocusState::Keyboard); });
        // Ctrl+1 .. Ctrl+9, Ctrl+0 select the views in order (as in pserv5)
        const auto &controllers = m_library.GetDataControllers();
        for (size_t i = 0; i < controllers.size() && i < 10; ++i)
        {
            const auto key = static_cast<VirtualKey>(static_cast<int>(VirtualKey::Number0) + static_cast<int>((i + 1) % 10));
            const std::string name{controllers[i]->GetControllerName()};
            add(key, VirtualKeyModifiers::Control, [this, name] { SelectView(name); });
        }
    }

    HWND MainWindow::WindowHandle()
    {
        HWND hwnd{nullptr};
        check_hresult(this->try_as<::IWindowNative>()->get_WindowHandle(&hwnd));
        return hwnd;
    }

    // The placement is the window's *normal* (restored) rectangle, which GetWindowPlacement reports
    // even while maximized, plus the maximized flag. Both directions use Get/SetWindowPlacement,
    // so the coordinates are the same kind (workspace) as pserv5 writes, which D13 imports.
    void MainWindow::RestoreWindowPlacement()
    {
        auto &window = pserv::config::theSettings.window;
        WINDOWPLACEMENT wp{sizeof(wp)};
        const HWND hwnd = WindowHandle();
        if (!GetWindowPlacement(hwnd, &wp))
        {
            LogWin32Error("GetWindowPlacement");
            return;
        }
        const int x = window.positionX.get();
        const int y = window.positionY.get();
        wp.rcNormalPosition = {x, y, x + std::max(640, window.width.get()), y + std::max(400, window.height.get())};
        wp.showCmd = SW_HIDE; // the window is shown by Activate()
        wp.flags = 0;
        if (!SetWindowPlacement(hwnd, &wp))
            LogWin32Error("SetWindowPlacement");
        if (window.maximized.get())
        {
            if (auto presenter = AppWindow().Presenter().try_as<Microsoft::UI::Windowing::OverlappedPresenter>())
                presenter.Maximize();
        }
    }

    void MainWindow::SaveSettings()
    {
        auto &settings = pserv::config::theSettings;
        WINDOWPLACEMENT wp{sizeof(wp)};
        if (GetWindowPlacement(WindowHandle(), &wp))
        {
            const RECT &rc = wp.rcNormalPosition;
            settings.window.maximized.set(wp.showCmd == SW_SHOWMAXIMIZED);
            settings.window.positionX.set(rc.left);
            settings.window.positionY.set(rc.top);
            settings.window.width.set(rc.right - rc.left);
            settings.window.height.set(rc.bottom - rc.top);
        }
        else
        {
            LogWin32Error("GetWindowPlacement");
        }
        if (m_pCurrent)
            settings.application.activeView.set(std::string{m_pCurrent->GetControllerName()});
        // Persisted by the App (BaseApp owns the backend) when the process exits
    }
}
