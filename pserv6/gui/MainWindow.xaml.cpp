#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif
#include "action_test.h"
#include "self_test.h"
#include <actions/common_actions.h>
#include <config/settings.h>
#include <controllers/services_data_controller.h>
#include <core/async_operation.h>
#include <core/data_controller.h>
#include <windows_api/service_manager.h>
#include <commctrl.h>
#include <utils/base_app.h>
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
        m_grid.m_onHeaderChanged = [this] { RebuildSortMenu(); };
        m_hwnd = WindowHandle();

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

        // Actions (plan Phase 5): dialogs, dispatch, progress. Completion of an asynchronous
        // operation arrives as WM_ASYNC_OPERATION_COMPLETE, received through a window subclass.
        ::pserv6::ActionRunner::Callbacks callbacks;
        callbacks.xamlRoot = [this] { return Content().XamlRoot(); };
        callbacks.hwnd = m_hwnd;
        callbacks.setRefreshPaused = [this](bool bPaused)
        {
            if (m_coordinator)
                m_coordinator->SetPaused(bPaused);
        };
        callbacks.refresh = [this](pserv::DataController *c) { RefreshAfterChange(c); };
        callbacks.status = [this](const std::string &text) { StatusText().Text(to_hstring(text)); };
        callbacks.isRemote = [this](const pserv::DataController *c) { return IsRemote(c); };
        callbacks.targetGeneration = [this] { return m_targetGeneration; };
        m_runner = std::make_unique<::pserv6::ActionRunner>(std::move(callbacks));
        if (!SetWindowSubclass(m_hwnd, &MainWindow::SubclassProc, 1, reinterpret_cast<DWORD_PTR>(this)))
            LogWin32Error("SetWindowSubclass");

        AddKeyboardShortcuts();
        RestoreWindowPlacement();
        BuildViewMenu();
        // Theme and zoom (plan Phase 6). The brushes are read once the theme resources are resolved,
        // and again whenever the actual theme changes (the user's choice or Windows' own setting).
        Root().ActualThemeChanged([this](auto &&, auto &&)
            {
                DispatcherQueue().TryEnqueue([this, alive = std::weak_ptr<bool>(m_alive)]
                    {
                        if (auto a = alive.lock(); a && *a)
                            ApplyThemeToChrome();
                    });
            });
        Root().Loaded([this](auto &&, auto &&) { ApplyThemeToChrome(); });
        ApplyTheme(settings.application.theme.get());
        SetZoomPercent(settings.application.fontSizeScaled.get() / 16);
        SetRemoteMachine(settings.application.serviceMachineName.get());

        Closed([this](auto &&, auto &&)
            {
                if (!m_selfTest) // a self-test run leaves the user's settings alone
                    SaveSettings();
                *m_alive = false;
                if (m_autoRefreshTimer)
                    m_autoRefreshTimer.Stop();
                if (m_runner)
                    m_runner->Shutdown();
                RemoveWindowSubclass(m_hwnd, &MainWindow::SubclassProc, 1);
                // Join the worker first: a running enumeration finishes and posts its completion to
                // the coordinator (which queues a UI task that m_alive turns into a no-op).
                m_worker.reset();
                m_coordinator.reset();
            });

        m_selfTest = ::pserv6::SelfTest::FromCommandLine(*this);
        if (!m_selfTest)
            m_actionTest = ::pserv6::ActionTest::FromCommandLine(*this);
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
        UpdateViewMenu();
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
        if (m_autoRefreshItem)
            m_autoRefreshItem.IsChecked(AutoRefreshSwitch().IsOn());
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

    // The keyboard path to sorting (the header cells are pointer-only): one radio item per column
    // and a Descending toggle; reached with Alt+S, then arrows and Enter.
    void MainWindow::RebuildSortMenu()
    {
        MenuFlyout flyout;
        flyout.Placement(Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);
        if (m_pCurrent)
        {
            const int sortColumn = m_pCurrent->GetLastSortColumn();
            const bool bAscending = m_pCurrent->IsLastSortAscending();
            const auto &columns = m_pCurrent->GetColumns();
            for (int c : m_grid.DisplayOrder())
            {
                RadioMenuFlyoutItem item;
                item.GroupName(L"SortColumn");
                item.Text(to_hstring(columns[static_cast<size_t>(c)].DisplayName));
                item.IsChecked(c == sortColumn);
                item.Click([this, c, bAscending, sortColumn](auto &&, auto &&) { m_grid.SetSort(c, sortColumn >= 0 ? bAscending : true); });
                flyout.Items().Append(item);
            }
            flyout.Items().Append(MenuFlyoutSeparator{});
            ToggleMenuFlyoutItem descending;
            descending.Text(L"Descending");
            descending.IsChecked(sortColumn >= 0 && !bAscending);
            descending.Click([this, sortColumn, bAscending](auto &&, auto &&) { m_grid.SetSort(sortColumn >= 0 ? sortColumn : 0, !bAscending); });
            flyout.Items().Append(descending);
        }
        SortButton().Flyout(flyout);
    }

    void MainWindow::CopySelection()
    {
        const auto selection = m_grid.GetSelection();
        spdlog::info("Copy: {} selected row(s)", selection.size());
        const std::string text = m_grid.FormatSelection();
        if (text.empty())
            return;
        try
        {
            pserv::utils::CopyToClipboard(text);
            StatusText().Text(to_hstring(std::format("Copied {} row(s)", selection.size())));
        }
        catch (const std::exception &e)
        {
            spdlog::error("Copy failed: {}", e.what());
        }
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
        add(VirtualKey::R, VirtualKeyModifiers::Control, [this] { AutoRefreshSwitch().IsOn(!AutoRefreshSwitch().IsOn()); });
        // Zoom: Ctrl+Plus / Ctrl+Minus (main block and number pad), and Ctrl+mouse wheel over the rows
        for (const auto key : {VirtualKey::Add, static_cast<VirtualKey>(VK_OEM_PLUS)})
            add(key, VirtualKeyModifiers::Control, [this] { SetZoomPercent(m_zoomPercent + 10); });
        for (const auto key : {VirtualKey::Subtract, static_cast<VirtualKey>(VK_OEM_MINUS)})
            add(key, VirtualKeyModifiers::Control, [this] { SetZoomPercent(m_zoomPercent - 10); });
        RowList().AddHandler(UIElement::PointerWheelChangedEvent(),
            box_value(Input::PointerEventHandler([this](auto &&, Input::PointerRoutedEventArgs const &args)
                {
                    if ((args.KeyModifiers() & VirtualKeyModifiers::Control) != VirtualKeyModifiers::Control)
                        return;
                    const int delta = args.GetCurrentPoint(nullptr).Properties().MouseWheelDelta();
                    if (delta != 0)
                        SetZoomPercent(m_zoomPercent + (delta > 0 ? 10 : -10));
                    args.Handled(true);
                })),
            true);

        // Keys that act on the selected rows: handled on the list itself, so they apply only while
        // it has focus (the filter box keeps its own Ctrl+C).
        RowList().KeyDown([this](auto &&, Input::KeyRoutedEventArgs const &args)
            {
                const bool bCtrl = (winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(VirtualKey::Control) &
                                    winrt::Windows::UI::Core::CoreVirtualKeyStates::Down) == winrt::Windows::UI::Core::CoreVirtualKeyStates::Down;
                if (bCtrl && args.Key() == VirtualKey::C)
                {
                    args.Handled(true);
                    CopySelection();
                }
                else if (args.Key() == VirtualKey::Enter && !bCtrl)
                {
                    args.Handled(true);
                    if (const auto selection = m_grid.GetSelection(); !selection.empty())
                        RunAction(&pserv::theDataPropertiesAction, selection);
                }
                else if (args.Key() == VirtualKey::Delete && m_pCurrent)
                {
                    // As in pserv5: the view's "Delete" action, if it has one for the selection
                    const auto selection = m_grid.GetSelection();
                    if (selection.empty())
                        return;
                    for (const auto *pAction : m_pCurrent->GetActions(selection.front()))
                    {
                        if (pAction->GetName() == "Delete" && pAction->IsAvailableFor(selection.front()))
                        {
                            args.Handled(true);
                            RunAction(pAction, selection);
                            break;
                        }
                    }
                }
            });
        // Ctrl+1 .. Ctrl+9, Ctrl+0 select the views in order (as in pserv5)
        const auto &controllers = m_library.GetDataControllers();
        for (size_t i = 0; i < controllers.size() && i < 10; ++i)
        {
            const auto key = static_cast<VirtualKey>(static_cast<int>(VirtualKey::Number0) + static_cast<int>((i + 1) % 10));
            const std::string name{controllers[i]->GetControllerName()};
            add(key, VirtualKeyModifiers::Control, [this, name] { SelectView(name); });
        }
    }

    LRESULT CALLBACK MainWindow::SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data)
    {
        if (msg == WM_ASYNC_OPERATION_COMPLETE)
        {
            if (auto *self = reinterpret_cast<MainWindow *>(data); self && self->m_runner && *self->m_alive)
                self->m_runner->OnAsyncComplete();
            return 0;
        }
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    void MainWindow::RunAction(const pserv::DataAction *pAction, const std::vector<pserv::DataObject *> &selection)
    {
        if (m_pCurrent && m_runner)
            m_runner->RequestAction(pAction, m_pCurrent, selection);
    }

    bool MainWindow::IsRemote(const pserv::DataController *pController) const
    {
        // Remote until the rows on screen are this machine's again: after Connect to Local
        // Machine the old rows stay until the new enumeration is published.
        const auto *services = dynamic_cast<const pserv::ServicesDataController *>(pController);
        return services && (!services->GetMachineName().empty() || !services->GetPublishedTarget().empty());
    }

    void MainWindow::RefreshAfterChange(pserv::DataController *pController)
    {
        if (!pController || !m_coordinator)
            return;
        // What an older enumeration found must not overwrite the change
        pController->InvalidatePendingRefreshes();
        m_coordinator->Request(pController);
    }

    void MainWindow::OnRowContextRequested(UIElement const &, Input::ContextRequestedEventArgs const &args)
    {
        args.Handled(true);
        pserv::DataObject *pClicked = nullptr;
        if (auto source = args.OriginalSource().try_as<DependencyObject>())
            pClicked = m_grid.ObjectFromContainer(source);
        winrt::Windows::Foundation::Point point{};
        std::optional<winrt::Windows::Foundation::Point> position;
        if (args.TryGetPosition(RowList(), point))
            position = point; // pointer; from the keyboard (Shift+F10, menu key) there is none
        ShowContextMenu(pClicked, RowList(), position);
    }

    void MainWindow::OnRowDoubleTapped(Windows::Foundation::IInspectable const &, Input::DoubleTappedRoutedEventArgs const &args)
    {
        auto source = args.OriginalSource().try_as<DependencyObject>();
        auto *pObject = source ? m_grid.ObjectFromContainer(source) : nullptr;
        if (!pObject)
            return;
        m_grid.SelectOnly(pObject);
        RunAction(&pserv::theDataPropertiesAction, {pObject});
    }

    // The row context menu (as pserv5's): the view's actions for the clicked object plus the
    // common copy/export actions, those available for the object, separators collapsed, the
    // selection count in multi-selection, destructive actions in the critical colour.
    void MainWindow::ShowContextMenu(pserv::DataObject *pClicked, UIElement const &target, std::optional<winrt::Windows::Foundation::Point> position)
    {
        if (!m_pCurrent || !m_runner || m_runner->IsBusy())
            return;
        auto selection = m_grid.GetSelection();
        if (pClicked && std::find(selection.begin(), selection.end(), pClicked) == selection.end())
        {
            m_grid.SelectOnly(pClicked); // right-click on an unselected row selects just it
            selection = {pClicked};
        }
        if (selection.empty())
            return;
        pserv::DataObject *pObject = pClicked ? pClicked : selection.front();

        // The menu may stay open across refreshes: it keeps its own references
        auto held = std::make_shared<::pserv6::RetainedSelection>(selection);
        auto *pController = m_pCurrent;
        const uint64_t generation = m_targetGeneration; // a click after a machine change is refused
        auto actions = pController->GetActions(pObject);
        pserv::AddCommonExportActions(actions);

        MenuFlyout menu;
        bool bLastWasSeparator = true; // no leading separator
        const auto criticalBrush = Application::Current().Resources().Lookup(box_value(L"SystemFillColorCriticalBrush")).as<Media::Brush>();
        for (const auto *pAction : actions)
        {
            if ((static_cast<int>(pAction->GetVisibility()) & static_cast<int>(pserv::ActionVisibility::ContextMenu)) == 0)
                continue;
            if (pAction->IsSeparator())
            {
                if (!bLastWasSeparator)
                    menu.Items().Append(MenuFlyoutSeparator{});
                bLastWasSeparator = true;
                continue;
            }
            if (!pAction->IsAvailableFor(pObject))
                continue;
            // A remote machine's objects: only what reads them (mutations would hit this machine)
            if (IsRemote(pController) && !::pserv6::ActionRunner::IsReadOnlyAction(pAction))
                continue;
            MenuFlyoutItem item;
            std::string label = pAction->GetName();
            if (selection.size() > 1)
                label += std::format(" ({} selected)", selection.size());
            item.Text(to_hstring(label));
            if (pAction->IsDestructive())
                item.Foreground(criticalBrush);
            item.Click([this, pAction, pController, held, generation](auto &&, auto &&)
                {
                    if (m_runner)
                        m_runner->RequestAction(pAction, pController, held->Get(), generation);
                });
            menu.Items().Append(item);
            bLastWasSeparator = false;
        }
        if (menu.Items().Size() > 0 && bLastWasSeparator)
            menu.Items().RemoveAtEnd(); // no trailing separator
        if (menu.Items().Size() == 0)
            return;
        Primitives::FlyoutShowOptions options;
        if (position)
            options.Position(*position);
        menu.ShowAt(target, options);
    }

    winrt::fire_and_forget MainWindow::OnConnectRemote(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        // The dialog takes the runner's single dialog slot and keeps it through the connection
        // test, so neither another dialog nor a second connect can interleave.
        if (!m_runner || !m_runner->ReserveDialog())
            co_return;
        std::weak_ptr<bool> alive = m_alive;
        const uint64_t generation = ++m_connectGeneration;
        ContentDialog dialog;
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Style>());
        dialog.Title(box_value(L"Connect to Remote Machine"));
        StackPanel panel;
        panel.Spacing(8);
        TextBox name;
        name.Header(box_value(L"Machine name or IP address"));
        name.Text(to_hstring(pserv::config::theSettings.application.serviceMachineName.get()));
        name.MinWidth(360);
        panel.Children().Append(name);
        TextBlock note;
        note.Text(L"Services and Devices are shown for that machine; the other views are unavailable while connected. "
                  L"Requires administrative rights on the target.");
        note.TextWrapping(TextWrapping::Wrap);
        note.Opacity(0.8);
        panel.Children().Append(note);
        dialog.Content(panel);
        dialog.PrimaryButtonText(L"Connect");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await dialog.ShowAsync();
        }
        catch (const winrt::hresult_error &e)
        {
            spdlog::error("Connect dialog failed: {}", winrt::to_string(e.message()));
        }
        if (auto a = alive.lock(); !a || !*a)
            co_return;
        const std::string machine = to_string(name.Text());
        if (result != ContentDialogResult::Primary || machine.empty())
        {
            m_runner->ReleaseDialog();
            co_return;
        }

        // Test the connection off the UI thread: an unreachable machine takes a while to fail
        StatusText().Text(to_hstring(std::format("Connecting to {}...", machine)));
        auto dispatcher = DispatcherQueue();
        co_await winrt::resume_background();
        const bool bConnected = pserv::ServiceManager{machine}.IsConnected();
        co_await wil::resume_foreground(dispatcher);
        if (auto a = alive.lock(); !a || !*a)
            co_return;
        m_runner->ReleaseDialog();
        if (generation != m_connectGeneration)
            co_return; // superseded (e.g. Connect to Local Machine meanwhile)
        if (!bConnected)
        {
            m_runner->ShowMessage(pserv::ActionMessageKind::Error, "Connect to Remote Machine",
                std::format("Could not connect to '{}'. Check the name or address, the network, and your rights on that machine.", machine));
            UpdateStatus();
            co_return;
        }
        SetRemoteMachine(machine);
    }

    void MainWindow::OnConnectLocal(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        ++m_connectGeneration; // a connection test still running must not switch back
        SetRemoteMachine({});
    }

    void MainWindow::OnExit(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        Close();
    }

    void MainWindow::SetRemoteMachine(const std::string &machineName)
    {
        if (pserv::config::theSettings.application.serviceMachineName.get() != machineName)
            ++m_targetGeneration; // menus, requests and dialogs opened before refuse to act afterwards
        pserv::config::theSettings.application.serviceMachineName.set(machineName);
        const bool bRemote = !machineName.empty();
        for (auto *controller : m_library.GetDataControllers())
        {
            // Services and Devices (a subclass) enumerate through the service control manager
            if (auto *services = dynamic_cast<pserv::ServicesDataController *>(controller))
            {
                if (services->GetMachineName() != machineName)
                    services->SetMachineName(machineName); // invalidates pending refreshes (target change)
            }
        }
        for (auto const &entry : Nav().MenuItems())
        {
            auto item = entry.as<NavigationViewItem>();
            const std::string name = to_string(unbox_value<hstring>(item.Tag()));
            const bool bSupported = name == pserv::SERVICES_DATA_CONTROLLER_NAME || name == pserv::DEVICES_DATA_CONTROLLER_NAME;
            item.IsEnabled(!bRemote || bSupported);
        }
        ConnectLocalItem().IsEnabled(bRemote);
        UpdateViewMenu();
        Title(bRemote ? to_hstring(std::format("pserv6 - {}", machineName)) : hstring{L"pserv6"});
        if (bRemote && m_pCurrent)
        {
            const std::string current{m_pCurrent->GetControllerName()};
            if (current != pserv::SERVICES_DATA_CONTROLLER_NAME && current != pserv::DEVICES_DATA_CONTROLLER_NAME)
            {
                SelectView(pserv::SERVICES_DATA_CONTROLLER_NAME);
                return;
            }
        }
        if (m_pCurrent)
            RequestRefresh(false);
    }

    // --- View menu, theme, zoom, Help (plan Phase 6) ---------------------------------------------

    void MainWindow::BuildViewMenu()
    {
        auto items = ViewMenu().Items();
        items.Clear();
        m_viewItems.clear();
        const auto &controllers = m_library.GetDataControllers();
        for (size_t i = 0; i < controllers.size(); ++i)
        {
            const std::string name{controllers[i]->GetControllerName()};
            RadioMenuFlyoutItem item;
            item.GroupName(L"Views");
            item.Text(to_hstring(name));
            if (i < 10)
                item.KeyboardAcceleratorTextOverride(to_hstring(std::format("Ctrl+{}", (i + 1) % 10)));
            item.Click([this, name](auto &&, auto &&) { SelectView(name); });
            items.Append(item);
            m_viewItems.push_back(item);
        }
        items.Append(MenuFlyoutSeparator{});

        auto &settings = pserv::config::theSettings;
        m_autoRefreshItem = ToggleMenuFlyoutItem{};
        m_autoRefreshItem.Text(L"Auto-refresh");
        m_autoRefreshItem.KeyboardAcceleratorTextOverride(L"Ctrl+R");
        m_autoRefreshItem.IsChecked(AutoRefreshSwitch().IsOn());
        m_autoRefreshItem.Click([this](auto &&, auto &&) { AutoRefreshSwitch().IsOn(!AutoRefreshSwitch().IsOn()); });
        items.Append(m_autoRefreshItem);

        MenuFlyoutSubItem interval;
        interval.Text(L"Refresh interval");
        const int currentMs = std::clamp(settings.autoRefresh.intervalMs.get(), 1000, 10000);
        for (const int seconds : {1, 2, 5, 10})
        {
            RadioMenuFlyoutItem item;
            item.GroupName(L"Interval");
            item.Text(to_hstring(std::format("{} second{}", seconds, seconds == 1 ? "" : "s")));
            item.IsChecked(currentMs == seconds * 1000);
            item.Click([this, seconds](auto &&, auto &&) { SetAutoRefreshInterval(seconds * 1000); });
            interval.Items().Append(item);
        }
        items.Append(interval);
        items.Append(MenuFlyoutSeparator{});

        MenuFlyoutSubItem theme;
        theme.Text(L"Theme");
        const std::string currentTheme = settings.application.theme.get();
        const bool bKnown = currentTheme == "Light" || currentTheme == "Dark";
        for (const auto &[label, value] : {std::pair{L"Use Windows setting", "System"}, std::pair{L"Light", "Light"}, std::pair{L"Dark", "Dark"}})
        {
            RadioMenuFlyoutItem item;
            item.GroupName(L"Theme");
            item.Text(label);
            item.IsChecked(bKnown ? currentTheme == value : std::string_view{value} == "System");
            item.Click([this, name = std::string{value}](auto &&, auto &&) { ApplyTheme(name); });
            theme.Items().Append(item);
        }
        items.Append(theme);

        MenuFlyoutSubItem zoom;
        zoom.Text(L"Zoom");
        MenuFlyoutItem zoomIn, zoomOut;
        zoomIn.Text(L"Zoom in");
        zoomIn.KeyboardAcceleratorTextOverride(L"Ctrl++");
        zoomIn.Click([this](auto &&, auto &&) { SetZoomPercent(m_zoomPercent + 10); });
        zoomOut.Text(L"Zoom out");
        zoomOut.KeyboardAcceleratorTextOverride(L"Ctrl+-");
        zoomOut.Click([this](auto &&, auto &&) { SetZoomPercent(m_zoomPercent - 10); });
        m_zoomResetItem = MenuFlyoutItem{};
        m_zoomResetItem.Click([this](auto &&, auto &&) { SetZoomPercent(100); });
        zoom.Items().Append(zoomIn);
        zoom.Items().Append(zoomOut);
        zoom.Items().Append(MenuFlyoutSeparator{});
        zoom.Items().Append(m_zoomResetItem);
        items.Append(zoom);
        UpdateViewMenu();
    }

    void MainWindow::UpdateViewMenu()
    {
        const auto &controllers = m_library.GetDataControllers();
        const bool bRemote = !pserv::config::theSettings.application.serviceMachineName.get().empty();
        for (size_t i = 0; i < m_viewItems.size() && i < controllers.size(); ++i)
        {
            const auto name = controllers[i]->GetControllerName();
            m_viewItems[i].IsChecked(controllers[i] == m_pCurrent);
            m_viewItems[i].IsEnabled(!bRemote || name == pserv::SERVICES_DATA_CONTROLLER_NAME || name == pserv::DEVICES_DATA_CONTROLLER_NAME);
        }
        if (m_zoomResetItem)
            m_zoomResetItem.Text(to_hstring(std::format("Reset to 100 % (now {} %)", m_zoomPercent)));
    }

    void MainWindow::ApplyTheme(const std::string &name)
    {
        const bool bKnown = name == "Light" || name == "Dark";
        pserv::config::theSettings.application.theme.set(bKnown ? name : std::string{"System"});
        const auto before = Root().ActualTheme();
        Root().RequestedTheme(name == "Light" ? ElementTheme::Light : (name == "Dark" ? ElementTheme::Dark : ElementTheme::Default));
        if (Root().ActualTheme() != before)
        {
            // The Mica backdrop takes its tint when it is attached and does not follow a later
            // change of the content's theme (executed: light Mica behind dark-theme text):
            // attach a new one.
            SystemBackdrop(nullptr);
            SystemBackdrop(Media::MicaBackdrop{});
        }
        ApplyThemeToChrome(); // ActualThemeChanged does not fire if the actual theme stays the same
    }

    void MainWindow::ApplyThemeToChrome()
    {
        ::pserv6::DataGrid::Brushes brushes;
        brushes.normal = BrushNormal().Foreground();
        brushes.highlight = BrushHighlight().Foreground();
        brushes.disabled = BrushDisabled().Foreground();
        brushes.gripper = BrushGripper().Background();
        brushes.hover = BrushHover().Background();
        if (!brushes.normal)
            return; // not resolved yet (before the content is loaded); Loaded calls again
        m_grid.SetBrushes(brushes);

        // The system-drawn title bar follows Windows' app mode unless told otherwise
        const BOOL bDark = Root().ActualTheme() == ElementTheme::Dark;
        const HRESULT hr = DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &bDark, sizeof(bDark));
        if (FAILED(hr))
            LogWin32ErrorCode("DwmSetWindowAttribute", hr, "DWMWA_USE_IMMERSIVE_DARK_MODE");
    }

    void MainWindow::SetZoomPercent(int percent)
    {
        percent = std::clamp(percent / 10 * 10, 50, 200);
        m_zoomPercent = percent;
        m_grid.SetZoom(percent / 100.0);
        pserv::config::theSettings.application.fontSizeScaled.set(percent * 16);
        UpdateViewMenu();
    }

    void MainWindow::SetAutoRefreshInterval(int milliseconds)
    {
        milliseconds = std::clamp(milliseconds, 1000, 10000);
        pserv::config::theSettings.autoRefresh.intervalMs.set(milliseconds);
        if (m_autoRefreshTimer)
            m_autoRefreshTimer.Interval(std::chrono::milliseconds(milliseconds));
    }

    void MainWindow::OnDocumentation(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        const auto result = reinterpret_cast<INT_PTR>(
            ShellExecuteW(m_hwnd, L"open", L"https://github.com/gersonkurz/pserv/blob/main/docs/pserv6.md", nullptr, nullptr, SW_SHOWNORMAL));
        if (result <= 32)
            m_runner->ShowMessage(pserv::ActionMessageKind::Error, "Documentation", "The documentation page could not be opened in a browser.");
    }

    winrt::fire_and_forget MainWindow::OnAbout(Windows::Foundation::IInspectable const &, RoutedEventArgs const &)
    {
        if (!m_runner || !m_runner->ReserveDialog())
            co_return;
        std::weak_ptr<bool> alive = m_alive;

        wil::unique_handle token;
        TOKEN_ELEVATION elevation{};
        DWORD size = 0;
        const bool bElevated = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put()) &&
                               GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
#if defined(_M_ARM64)
        const char *architecture = "ARM64";
#else
        const char *architecture = "x64";
#endif
#ifdef _DEBUG
        const char *configuration = "Debug";
#else
        const char *configuration = "Release";
#endif

        ContentDialog dialog;
        dialog.XamlRoot(Content().XamlRoot());
        dialog.RequestedTheme(Root().ActualTheme());
        dialog.Style(Application::Current().Resources().Lookup(box_value(L"DefaultContentDialogStyle")).as<Style>());
        dialog.Title(box_value(L"About pserv6"));
        StackPanel panel;
        panel.Spacing(6);
        auto line = [&panel](const std::string &text, bool bStrong = false)
        {
            TextBlock block;
            block.Text(to_hstring(text));
            block.TextWrapping(TextWrapping::Wrap);
            block.IsTextSelectionEnabled(true);
            if (bStrong)
                block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            panel.Children().Append(block);
        };
        line(std::format("pserv6 {} ({}, {})", PSERV_VERSION_STRING, architecture, configuration), true);
        line("Windows system administration tool: services, devices, processes, windows, modules, installed programs, "
             "environment variables, startup programs, network connections and scheduled tasks.");
        line(PSERV_COPYRIGHT);
        line(std::format("Built {} {}", __DATE__, __TIME__));
        line(std::format("Running as administrator: {}", bElevated ? "yes" : "no (some data and actions are unavailable)"));
        line(std::format("Settings and logs: {}", pserv::utils::WideToUtf8(pserv::utils::GetConfigFolder().wstring())));
        HyperlinkButton link;
        link.Content(box_value(L"github.com/gersonkurz/pserv"));
        link.NavigateUri(Windows::Foundation::Uri{L"https://github.com/gersonkurz/pserv"});
        link.Padding(ThicknessHelper::FromUniformLength(0));
        panel.Children().Append(link);
        dialog.Content(panel);
        dialog.CloseButtonText(L"Close");
        dialog.DefaultButton(ContentDialogButton::Close);
        try
        {
            co_await dialog.ShowAsync();
        }
        catch (const winrt::hresult_error &e)
        {
            spdlog::error("About dialog failed: {}", winrt::to_string(e.message()));
        }
        if (auto a = alive.lock(); !a || !*a)
            co_return;
        m_runner->ReleaseDialog();
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
