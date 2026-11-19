#include "precomp.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "Resource.h"
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#include <actions/common_actions.h>
#include <config/settings.h>
#include <controllers/environment_variables_data_controller.h>
#include <controllers/services_data_controller.h>
#include <core/async_operation.h>
#include <core/data_action.h>
#include <core/data_controller.h>
#include <core/data_object.h>
#include <models/environment_variable_info.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/environment_variable_manager.h>
#include <windows_api/service_manager.h>
#include <winrt/Microsoft.UI.Interop.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
using namespace winrt::Microsoft::UI::Xaml::Input;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;

namespace winrt::pserv6::implementation
{
    namespace
    {
        winrt::Windows::UI::Color ToWinUIColor(const pserv::theme::Color4 &c, uint8_t alphaOverride = 255)
        {
            return winrt::Windows::UI::ColorHelper::FromArgb(alphaOverride, c.R(), c.G(), c.B());
        }

        const wchar_t *GetControllerIconGlyph(std::string_view name)
        {
            if (name == pserv::SERVICES_DATA_CONTROLLER_NAME) return L"\xE912";            // Gear / Services
            if (name == pserv::DEVICES_DATA_CONTROLLER_NAME) return L"\xE964";             // Hardware / Devices
            if (name == pserv::PROCESSES_DATA_CONTROLLER_NAME) return L"\xE9D9";           // Activity / Processes
            if (name == pserv::WINDOWS_DATA_CONTROLLER_NAME) return L"\xE737";             // Window
            if (name == pserv::UNINSTALLER_DATA_CONTROLLER_NAME) return L"\xE74C";         // Apps / Uninstaller
            if (name == pserv::STARTUP_PROGRAMS_DATA_CONTROLLER_NAME) return L"\xE7E8";    // Power / Startup
            if (name == pserv::NETWORK_CONNECTIONS_DATA_CONTROLLER_NAME) return L"\xE968"; // Network
            if (name == pserv::SCHEDULED_TASKS_DATA_CONTROLLER_NAME) return L"\xE823";     // Clock / Tasks
            if (name == pserv::MODULES_DATA_CONTROLLER_NAME) return L"\xE74C";             // Library / Modules
            if (name == pserv::ENVIRONMENT_VARIABLES_CONTROLLER_NAME) return L"\xE943";    // Code / Env Vars
            return L"\xE8A5";
        }

        const wchar_t *GetActionIconGlyph(std::string_view name)
        {
            if (name == "Start" || name == "Run Now") return L"\xE768";                 // Play
            if (name == "Stop") return L"\xE71A";                                       // Stop
            if (name == "Restart") return L"\xE777";                                    // Sync / Restart
            if (name == "Pause") return L"\xE769";                                      // Pause
            if (name == "Resume") return L"\xE893";                                     // Resume / Next
            if (name == "Enable") return L"\xE73E";                                     // CheckMark
            if (name == "Disable") return L"\xF140";                                    // Blocked
            if (name == "Delete" || name == "Uninstall" || name == "Uninstall Service" || name == "Delete Registry Key")
                return L"\xE74D";                                                       // Delete
            if (name == "Terminate Process") return L"\xE7E8";                          // Power
            if (name == "Close" || name == "Close Connection") return L"\xE711";        // Cancel / Close
            if (name == "Open in Registry Editor") return L"\xE90F";                    // Repair / Registry
            if (name == "Open in Explorer" || name == "Open File Location" || name == "Open Containing Folder")
                return L"\xE838";                                                       // FolderOpen
            if (name == "Open Terminal Here") return L"\xE756";                         // CommandPrompt
            if (name == "Edit Configuration") return L"\xE70F";                         // Edit
            if (name == "Show") return L"\xE7B3";                                       // View
            if (name == "Hide") return L"\xED1A";                                       // Hide
            if (name == "Minimize") return L"\xE921";                                   // ChromeMinimize
            if (name == "Maximize") return L"\xE922";                                   // ChromeMaximize
            if (name == "Restore") return L"\xE923";                                    // ChromeRestore
            if (name == "Bring To Front") return L"\xE8A7";                             // OpenInNewWindow
            if (name.starts_with("Copy")) return L"\xE8C8";                             // Copy
            if (name.starts_with("Add")) return L"\xE710";                              // Add
            if (name.starts_with("Export")) return L"\xE74E";                           // Save / Export
            if (name == "Properties") return L"\xE946";                                 // Info / Properties
            return L"\xE713";                                                           // Settings fallback
        }
    } // namespace

    MainWindow::MainWindow()
        : m_lastAutoRefreshTime{std::chrono::steady_clock::now()}
    {
        try
        {
            spdlog::info("MainWindow::MainWindow: Calling InitializeComponent()...");
            InitializeComponent();
            spdlog::info("MainWindow::MainWindow: Calling InitializeWindow()...");
            InitializeWindow();
            spdlog::info("MainWindow::MainWindow: Initialization complete");
        }
        catch (const winrt::hresult_error &ex)
        {
            spdlog::error("MainWindow ctor hresult_error: 0x{:08X} - {}", static_cast<uint32_t>(ex.code()), winrt::to_string(ex.message()));
            throw;
        }
        catch (const std::exception &ex)
        {
            spdlog::error("MainWindow ctor std::exception: {}", ex.what());
            throw;
        }
    }

    MainWindow::~MainWindow()
    {
        if (m_periodicTimer)
        {
            m_periodicTimer.Stop();
        }
        if (m_hWnd)
        {
            RemoveWindowSubclass(m_hWnd, SubclassWndProc, 1);
        }
        for (auto *obj : m_dispatchContext.m_selectedObjects)
        {
            obj->Release(REFCOUNT_DEBUG_ARGS);
        }
        m_dispatchContext.m_selectedObjects.clear();
        m_controllers.Clear();
    }

    void MainWindow::InitializeWindow()
    {
        // Retrieve underlying Win32 HWND
        auto windowNative = this->try_as<::IWindowNative>();
        if (windowNative)
        {
            windowNative->get_WindowHandle(&m_hWnd);
        }
        m_dispatchContext.m_hWnd = m_hWnd;

        // Subclass HWND to receive WM_ASYNC_OPERATION_COMPLETE and WM_DESTROY
        if (m_hWnd)
        {
            SetWindowSubclass(m_hWnd, SubclassWndProc, 1, reinterpret_cast<DWORD_PTR>(this));

            HINSTANCE hInst = GetModuleHandleW(nullptr);
            HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_PSERV6));
            if (hIcon)
            {
                SendMessageW(m_hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
                SendMessageW(m_hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
            }
        }

        // Configure AppWindow & TitleBar + Mica Backdrop
        try
        {
            if (winrt::Microsoft::UI::Composition::SystemBackdrops::MicaController::IsSupported())
            {
                winrt::Microsoft::UI::Xaml::Media::MicaBackdrop micaBackdrop;
                this->SystemBackdrop(micaBackdrop);
            }
            else if (winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController::IsSupported())
            {
                winrt::Microsoft::UI::Xaml::Media::DesktopAcrylicBackdrop acrylicBackdrop;
                this->SystemBackdrop(acrylicBackdrop);
            }
        }
        catch (...)
        {
            spdlog::warn("SystemBackdrop initialization skipped");
        }

        this->Title(winrt::to_hstring("pserv6 " PSERV_VERSION_STRING));
        AppTitleTextBlock().Text(winrt::to_hstring("pserv6 " PSERV_VERSION_STRING));

        try
        {
            this->ExtendsContentIntoTitleBar(true);
            this->SetTitleBar(AppTitleBar());
        }
        catch (...)
        {
        }

        if (m_hWnd)
        {
            m_appWindow = this->AppWindow();

            // Restore window position & size from settings
            auto &winSettings = pserv::config::theSettings.window;
            int width = std::clamp(winSettings.width.get(), 800, 3840);
            int height = std::clamp(winSettings.height.get(), 500, 2160);
            int posX = std::clamp(winSettings.positionX.get(), 20, 1400);
            int posY = std::clamp(winSettings.positionY.get(), 20, 900);
            bool maximized = winSettings.maximized.get();

            SetWindowPos(m_hWnd, nullptr, posX, posY, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            if (maximized)
            {
                ShowWindow(m_hWnd, SW_SHOWMAXIMIZED);
            }
        }

        // Load font size from config
        int32_t fontSizeScaled = pserv::config::theSettings.application.fontSizeScaled.get();
        m_fontSize = std::clamp(fontSizeScaled / 100.0, 9.0, 28.0);

        // Load Base16 themes, populate Themes menu once, and apply configured theme
        m_themeSet.LoadFromDirectory(GetThemesDirectory());
        PopulateThemesMenu();
        ApplyTheme(pserv::config::theSettings.application.theme.get());

        // Initialize persistent observable vector for zero-flicker in-place list updates
        m_listItems = winrt::single_threaded_observable_vector<IInspectable>();
        DataListView().ItemsSource(m_listItems);

        // Catch Ctrl+MouseWheel anywhere in the window (including when ListView's ScrollViewer handles it)
        RootGrid().AddHandler(
            UIElement::PointerWheelChangedEvent(),
            box_value(PointerEventHandler{this, &MainWindow::OnRootPointerWheelChanged}),
            true);

        // Restore remote machine name if set
        const auto &controllers = m_controllers.GetDataControllers();
        if (!controllers.empty())
        {
            auto *servicesCtrl = static_cast<pserv::ServicesDataController *>(controllers[0]);
            std::string savedMachine = pserv::config::theSettings.application.serviceMachineName.get();
            if (!savedMachine.empty())
            {
                servicesCtrl->SetMachineName(savedMachine);
            }
        }

        // Populate navigation items & view menu
        PopulateNavigationAndMenus();
        UpdateRemoteModeUI();

        // Hook horizontal scroll synchronization once DataListView loads
        DataListView().Loaded([this](IInspectable const &, RoutedEventArgs const &)
        {
            SyncListScrollViewer();
        });

        // Start periodic timer for auto-refresh and async operation progress polling
        m_periodicTimer = DispatcherTimer();
        m_periodicTimer.Interval(std::chrono::milliseconds(100));
        m_periodicTimer.Tick([this](IInspectable const &, IInspectable const &)
        {
            OnTimerTick();
        });
        m_periodicTimer.Start();
    }

    void MainWindow::BringToFront()
    {
        if (m_hWnd)
        {
            if (m_appWindow)
            {
                try
                {
                    m_appWindow.Show();
                }
                catch (...)
                {
                }
            }
            ShowWindow(m_hWnd, IsIconic(m_hWnd) ? SW_RESTORE : SW_SHOWNORMAL);
            SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            SetWindowPos(m_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            SetForegroundWindow(m_hWnd);
            BringWindowToTop(m_hWnd);
        }
    }

    LRESULT CALLBACK MainWindow::SubclassWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR dwRefData)
    {
        auto *pThis = reinterpret_cast<MainWindow *>(dwRefData);
        if (pThis)
        {
            if (uMsg == WM_ASYNC_OPERATION_COMPLETE)
            {
                pThis->OnAsyncOperationComplete();
                return 0;
            }
            else if (uMsg == WM_CLOSE)
            {
                pThis->SaveWindowState();
                pThis->SaveCurrentTableState();
            }
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    std::wstring MainWindow::GetThemesDirectory() const
    {
        wchar_t exePath[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len == 0 || len == MAX_PATH)
        {
            return L"themes";
        }
        std::filesystem::path path{exePath};
        path.replace_filename(L"themes");
        return path.wstring();
    }

    void MainWindow::ApplyTheme(const std::string &name)
    {
        if (!m_themeSet.SelectByName(name))
        {
            for (const auto &info : m_themeSet.Themes())
            {
                if (info.dark && m_themeSet.SelectByName(info.name))
                    break;
            }
        }

        const auto &scheme = m_themeSet.Current();
        RootGrid().RequestedTheme(scheme.isDark ? ElementTheme::Dark : ElementTheme::Light);

        // Build cached SolidColorBrushes from the Base16 palette
        m_normalTextBrush = SolidColorBrush(ToWinUIColor(scheme.base[0x05]));
        m_highlightTextBrush = SolidColorBrush(ToWinUIColor(scheme.base[0x0D]));
        m_disabledTextBrush = SolidColorBrush(ToWinUIColor(scheme.base[0x03]));
        m_altRowBrush = SolidColorBrush(ToWinUIColor(scheme.base[0x01], 95));
        m_transparentBrush = SolidColorBrush(winrt::Windows::UI::Colors::Transparent());
        m_destructiveBrush = SolidColorBrush(ToWinUIColor(scheme.base[0x08]));

        // Apply subtle Base16 background tint to main content and status bar
        ContentAreaGrid().Background(SolidColorBrush(ToWinUIColor(scheme.base[0x00], 215)));
        HeaderScrollViewer().Background(SolidColorBrush(ToWinUIColor(scheme.base[0x01], 240)));
        StatusBarBorder().Background(SolidColorBrush(ToWinUIColor(scheme.base[0x01], 240)));
        TopToolbarGrid().Background(SolidColorBrush(ToWinUIColor(scheme.base[0x01], 180)));

        // Re-tint visible rows and toolbar icons, and sync theme checkmarks (without rebuilding menu items)
        RefreshVisibleRowsInPlace();
        m_lastToolbarActions.clear();
        UpdateQuickActionButtons();
        UpdateThemesMenuChecks();
    }

    void MainWindow::PopulateThemesMenu()
    {
        auto items = ThemesMenuBarItem().Items();
        items.Clear();
        m_themeMenuItems.clear();

        const std::string currentName = m_themeSet.Current().name;
        const auto allThemes = m_themeSet.Themes();

        auto addGroup = [&](const wchar_t *headerTitle, bool darkGroup)
        {
            MenuFlyoutSubItem subMenu;
            subMenu.Text(headerTitle);

            for (const auto &info : allThemes)
            {
                if (info.dark != darkGroup)
                    continue;

                ToggleMenuFlyoutItem item;
                item.Text(winrt::to_hstring(info.name));
                item.IsChecked(info.name == currentName);
                std::string capturedName = info.name;
                item.Click([this, capturedName](IInspectable const &, RoutedEventArgs const &)
                {
                    pserv::config::theSettings.application.theme.set(capturedName);
                    if (m_pConfigBackend)
                    {
                        pserv::config::theSettings.application.save(*m_pConfigBackend);
                    }
                    ApplyTheme(capturedName);
                });
                subMenu.Items().Append(item);
                m_themeMenuItems.emplace_back(info.name, item);
            }
            items.Append(subMenu);
        };

        addGroup(L"Dark Themes", true);
        addGroup(L"Light Themes", false);
    }

    void MainWindow::UpdateThemesMenuChecks()
    {
        const std::string currentName = m_themeSet.Current().name;
        for (auto &[name, item] : m_themeMenuItems)
        {
            item.IsChecked(name == currentName);
        }
    }

    void MainWindow::PopulateNavigationAndMenus()
    {
        auto navItems = MainNavView().MenuItems();
        navItems.Clear();

        const auto &controllers = m_controllers.GetDataControllers();
        std::string savedActiveView = pserv::config::theSettings.application.activeView.get();
        NavigationViewItem selectedNavItem{nullptr};

        int index = 1;
        for (auto *controller : controllers)
        {
            const std::string &ctrlName = controller->GetControllerName();

            NavigationViewItem navItem;
            navItem.Content(box_value(winrt::to_hstring(ctrlName)));
            navItem.Tag(box_value(winrt::to_hstring(ctrlName)));

            FontIcon icon;
            icon.Glyph(GetControllerIconGlyph(ctrlName));
            navItem.Icon(icon);

            navItems.Append(navItem);

            if (ctrlName == savedActiveView || !selectedNavItem)
            {
                selectedNavItem = navItem;
            }

            // Also add to View MenuBarItem
            MenuFlyoutItem viewMenuItem;
            viewMenuItem.Text(winrt::to_hstring(std::format("{} (Ctrl+{})", ctrlName, index % 10)));
            viewMenuItem.Tag(box_value(winrt::to_hstring(ctrlName)));
            viewMenuItem.Click([this, controller](IInspectable const &, RoutedEventArgs const &)
            {
                SwitchToController(controller);
            });
            ViewMenuBarItem().Items().Append(viewMenuItem);
            ++index;
        }

        // Sync Auto-Refresh menu checks
        bool autoRefreshEnabled = pserv::config::theSettings.autoRefresh.enabled.get();
        int32_t intervalMs = pserv::config::theSettings.autoRefresh.intervalMs.get();
        MenuAutoRefreshToggle().IsChecked(autoRefreshEnabled);
        MenuInterval1s().IsChecked(intervalMs == 1000);
        MenuInterval2s().IsChecked(intervalMs == 2000);
        MenuInterval5s().IsChecked(intervalMs == 5000);
        MenuInterval10s().IsChecked(intervalMs == 10000);

        if (selectedNavItem)
        {
            MainNavView().SelectedItem(selectedNavItem);
        }
    }

    void MainWindow::UpdateRemoteModeUI()
    {
        std::string machine = pserv::config::theSettings.application.serviceMachineName.get();
        bool isRemote = !machine.empty();

        MenuConnectLocalItem().IsChecked(!isRemote);
        MenuConnectRemoteItem().IsChecked(isRemote);

        if (isRemote)
        {
            ConnectionBadgeText().Text(winrt::to_hstring(std::format("Remote: {}", machine)));
        }
        else
        {
            ConnectionBadgeText().Text(L"Local Machine");
        }

        // Enable/disable navigation items based on remote mode
        for (auto const &itemObj : MainNavView().MenuItems())
        {
            if (auto navItem = itemObj.try_as<NavigationViewItem>())
            {
                std::string name = winrt::to_string(unbox_value_or<winrt::hstring>(navItem.Tag(), L""));
                bool enabled = !isRemote || (name == pserv::SERVICES_DATA_CONTROLLER_NAME || name == pserv::DEVICES_DATA_CONTROLLER_NAME);
                navItem.IsEnabled(enabled);
            }
        }
    }

    void MainWindow::OnNavSelectionChanged(NavigationView const &, NavigationViewSelectionChangedEventArgs const &args)
    {
        if (m_bIsSwitchingController)
            return;

        if (auto navItem = args.SelectedItem().try_as<NavigationViewItem>())
        {
            std::string targetName = winrt::to_string(unbox_value_or<winrt::hstring>(navItem.Tag(), L""));
            for (auto *controller : m_controllers.GetDataControllers())
            {
                if (controller->GetControllerName() == targetName)
                {
                    SwitchToController(controller);
                    break;
                }
            }
        }
    }

    void MainWindow::SwitchToController(pserv::DataController *controller)
    {
        if (!controller || m_pCurrentController == controller)
            return;

        // Save previous controller's table column state before switching
        if (m_pCurrentController && m_bTableDirty)
        {
            SaveCurrentTableState();
        }

        m_bIsSwitchingController = true;
        m_pCurrentController = controller;
        m_dispatchContext.m_pController = controller;

        // Clear current selection
        for (auto *obj : m_dispatchContext.m_selectedObjects)
        {
            obj->Release(REFCOUNT_DEBUG_ARGS);
        }
        m_dispatchContext.m_selectedObjects.clear();

        const std::string &ctrlName = controller->GetControllerName();

        // Sync NavigationView selection if triggered from shortcut or menu
        for (auto const &itemObj : MainNavView().MenuItems())
        {
            if (auto navItem = itemObj.try_as<NavigationViewItem>())
            {
                std::string tag = winrt::to_string(unbox_value_or<winrt::hstring>(navItem.Tag(), L""));
                if (tag == ctrlName)
                {
                    MainNavView().SelectedItem(navItem);
                    break;
                }
            }
        }

        // Persist active view to settings
        pserv::config::theSettings.application.activeView.set(ctrlName);
        if (m_pConfigBackend)
        {
            pserv::config::theSettings.application.save(*m_pConfigBackend);
        }

        // Update UI placeholders & tooltips
        ToolTipService::SetToolTip(RefreshButton(), box_value(winrt::to_hstring(std::format("Refresh {} (F5)", ctrlName))));
        FilterTextBox().PlaceholderText(winrt::to_hstring(std::format("Filter {}... (Ctrl+F)", ctrlName)));

        // Ensure data is loaded
        if (!controller->IsLoaded())
        {
            try
            {
                controller->Refresh(false);
            }
            catch (const std::exception &e)
            {
                spdlog::error("Initial refresh failed for {}: {}", ctrlName, e.what());
            }
        }

        // Load saved column widths & sort order
        LoadColumnStateForController(controller);
        BuildColumnHeaders();
        m_lastToolbarActions.clear();
        RebuildFilteredList(false, true);
        UpdateQuickActionButtons();
        UpdateStatusBar();

        m_bIsSwitchingController = false;
    }

    void MainWindow::LoadColumnStateForController(pserv::DataController *controller)
    {
        m_columnWidths.clear();
        if (!controller)
            return;

        const auto &columns = controller->GetColumns();
        auto *section = pserv::config::theSettings.getSectionFor(controller->GetControllerName());
        if (section)
        {
            std::istringstream iss{section->columnWidths.get()};
            std::string token;
            while (std::getline(iss, token, ','))
            {
                try
                {
                    double w = std::stod(token);
                    m_columnWidths.push_back(std::clamp(w, 45.0, 1200.0));
                }
                catch (...)
                {
                    m_columnWidths.push_back(140.0);
                }
            }

            int32_t savedSortCol = section->sortColumn.get();
            bool savedSortAsc = section->sortAscending.get();
            if (savedSortCol >= 0 && savedSortCol < static_cast<int32_t>(columns.size()))
            {
                controller->Sort(savedSortCol, savedSortAsc);
            }
            else if (controller->GetLastSortColumn() < 0 && !columns.empty())
            {
                controller->Sort(0, true);
            }
        }

        while (m_columnWidths.size() < columns.size())
        {
            m_columnWidths.push_back(140.0);
        }
        m_bTableDirty = false;
    }

    void MainWindow::SaveCurrentTableState()
    {
        if (!m_pCurrentController || !m_pConfigBackend || m_columnWidths.empty())
            return;

        auto *section = pserv::config::theSettings.getSectionFor(m_pCurrentController->GetControllerName());
        if (!section)
            return;

        std::ostringstream oss;
        for (size_t i = 0; i < m_columnWidths.size(); ++i)
        {
            if (i > 0)
                oss << ",";
            oss << static_cast<int>(m_columnWidths[i]);
        }
        section->columnWidths.set(oss.str());
        section->sortColumn.set(m_pCurrentController->GetLastSortColumn());
        section->sortAscending.set(m_pCurrentController->GetLastSortAscending());
        section->save(*m_pConfigBackend);
        m_bTableDirty = false;
    }

    void MainWindow::SaveWindowState()
    {
        if (!m_hWnd || !m_pConfigBackend)
            return;

        WINDOWPLACEMENT wp{};
        wp.length = sizeof(WINDOWPLACEMENT);
        if (!GetWindowPlacement(m_hWnd, &wp))
            return;

        auto &winSettings = pserv::config::theSettings.window;
        winSettings.maximized.set(wp.showCmd == SW_SHOWMAXIMIZED);
        RECT &rc = wp.rcNormalPosition;
        winSettings.positionX.set(rc.left);
        winSettings.positionY.set(rc.top);
        winSettings.width.set(rc.right - rc.left);
        winSettings.height.set(rc.bottom - rc.top);
        winSettings.save(*m_pConfigBackend);
    }

    void MainWindow::BuildColumnHeaders()
    {
        ++m_currentColumnSchemaId;
        auto headerChildren = HeaderStackPanel().Children();
        headerChildren.Clear();

        if (!m_pCurrentController)
            return;

        const auto &columns = m_pCurrentController->GetColumns();
        int sortCol = m_pCurrentController->GetLastSortColumn();
        bool sortAsc = m_pCurrentController->GetLastSortAscending();

        for (size_t i = 0; i < columns.size(); ++i)
        {
            const auto &col = columns[i];
            double colWidth = (i < m_columnWidths.size()) ? m_columnWidths[i] : 140.0;

            Grid headerCell;
            headerCell.Width(colWidth);
            headerCell.Height(34);

            ColumnDefinition c0;
            c0.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            ColumnDefinition c1;
            c1.Width(GridLengthHelper::FromPixels(6));
            headerCell.ColumnDefinitions().Append(c0);
            headerCell.ColumnDefinitions().Append(c1);

            Button sortBtn;
            sortBtn.HorizontalAlignment(HorizontalAlignment::Stretch);
            sortBtn.VerticalAlignment(VerticalAlignment::Stretch);
            sortBtn.HorizontalContentAlignment(
                col.GetAlignment() == pserv::ColumnAlignment::Right ? HorizontalAlignment::Right : HorizontalAlignment::Left);
            sortBtn.Background(m_transparentBrush);
            sortBtn.BorderThickness(ThicknessHelper::FromUniformLength(0));
            sortBtn.CornerRadius(CornerRadiusHelper::FromUniformRadius(0));
            sortBtn.Padding(ThicknessHelper::FromLengths(8, 0, 8, 0));

            StackPanel labelPanel;
            labelPanel.Orientation(Orientation::Horizontal);
            labelPanel.Spacing(6);

            TextBlock titleText;
            titleText.Text(winrt::to_hstring(col.DisplayName));
            titleText.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
            titleText.FontSize(m_fontSize);
            titleText.VerticalAlignment(VerticalAlignment::Center);
            titleText.TextTrimming(TextTrimming::CharacterEllipsis);
            labelPanel.Children().Append(titleText);

            if (static_cast<int>(i) == sortCol)
            {
                FontIcon sortIcon;
                sortIcon.Glyph(sortAsc ? L"\xE70E" : L"\xE70D"); // ChevronUp / ChevronDown
                sortIcon.FontSize(10);
                sortIcon.VerticalAlignment(VerticalAlignment::Center);
                labelPanel.Children().Append(sortIcon);
            }

            sortBtn.Content(labelPanel);

            int capturedColIndex = static_cast<int>(i);
            sortBtn.Click([this, capturedColIndex](IInspectable const &, RoutedEventArgs const &)
            {
                if (!m_pCurrentController)
                    return;

                int currentSort = m_pCurrentController->GetLastSortColumn();
                bool currentAsc = m_pCurrentController->GetLastSortAscending();
                bool newAsc = (currentSort == capturedColIndex) ? !currentAsc : true;

                m_pCurrentController->Sort(capturedColIndex, newAsc);
                m_bTableDirty = true;
                SaveCurrentTableState();
                BuildColumnHeaders();
                RebuildFilteredList(true);
            });

            Grid::SetColumn(sortBtn, 0);
            headerCell.Children().Append(sortBtn);

            // Column Resize Thumb
            Thumb resizeThumb;
            resizeThumb.Width(6);
            resizeThumb.HorizontalAlignment(HorizontalAlignment::Right);
            resizeThumb.VerticalAlignment(VerticalAlignment::Stretch);
            resizeThumb.Background(SolidColorBrush(ToWinUIColor(m_themeSet.Current().base[0x02], 140)));

            size_t capturedIdx = i;
            resizeThumb.DragDelta([this, capturedIdx, headerCell](IInspectable const &, DragDeltaEventArgs const &e)
            {
                if (capturedIdx >= m_columnWidths.size())
                    return;

                double newWidth = std::clamp(m_columnWidths[capturedIdx] + e.HorizontalChange(), 45.0, 1200.0);
                m_columnWidths[capturedIdx] = newWidth;
                headerCell.Width(newWidth);
                m_bTableDirty = true;

                // Resize corresponding column in visible rows immediately
                if (auto itemsPanel = DataListView().ItemsPanelRoot())
                {
                    for (auto const &child : itemsPanel.Children())
                    {
                        if (auto lvi = child.try_as<ListViewItem>())
                        {
                            Grid rowGrid = lvi.ContentTemplateRoot().try_as<Grid>();
                            if (!rowGrid)
                            {
                                rowGrid = lvi.Content().try_as<Grid>();
                            }
                            if (rowGrid)
                            {
                                auto colDefs = rowGrid.ColumnDefinitions();
                                if (capturedIdx < colDefs.Size())
                                {
                                    colDefs.GetAt(static_cast<uint32_t>(capturedIdx)).Width(GridLengthHelper::FromPixels(newWidth));
                                }
                            }
                        }
                    }
                }
            });

            resizeThumb.DragCompleted([this](IInspectable const &, DragCompletedEventArgs const &)
            {
                SaveCurrentTableState();
            });

            Grid::SetColumn(resizeThumb, 1);
            headerCell.Children().Append(resizeThumb);

            headerChildren.Append(headerCell);
        }
    }

    void MainWindow::SyncListScrollViewer()
    {
        if (m_listScrollViewer)
            return;

        // Walk visual tree of DataListView to find its internal ScrollViewer
        DependencyObject root = DataListView();
        std::vector<DependencyObject> queue{root};
        while (!queue.empty())
        {
            DependencyObject current = queue.back();
            queue.pop_back();

            if (auto sv = current.try_as<ScrollViewer>())
            {
                m_listScrollViewer = sv;
                m_listScrollViewer.ViewChanged([this](IInspectable const &, ScrollViewerViewChangedEventArgs const &)
                {
                    if (m_listScrollViewer)
                    {
                        HeaderScrollViewer().ChangeView(m_listScrollViewer.HorizontalOffset(), nullptr, nullptr, true);
                    }
                });
                break;
            }

            int count = VisualTreeHelper::GetChildrenCount(current);
            for (int i = 0; i < count; ++i)
            {
                queue.push_back(VisualTreeHelper::GetChild(current, i));
            }
        }
    }

    void MainWindow::RebuildFilteredList(bool preserveSelection, bool forceReset)
    {
        if (!m_pCurrentController)
            return;

        // Collect currently selected stable IDs if preserving selection
        std::unordered_set<std::string> selectedIds;
        if (preserveSelection)
        {
            for (const auto *obj : m_dispatchContext.m_selectedObjects)
            {
                selectedIds.insert(obj->GetStableID());
            }
        }

        // Release old selection references
        for (auto *obj : m_dispatchContext.m_selectedObjects)
        {
            obj->Release(REFCOUNT_DEBUG_ARGS);
        }
        m_dispatchContext.m_selectedObjects.clear();

        // Filter objects
        m_filteredObjects.clear();
        const auto &allObjects = m_pCurrentController->GetDataObjects();
        std::string filterStr = winrt::to_string(FilterTextBox().Text());

        if (!filterStr.empty())
        {
            std::string lowerFilter = pserv::utils::ToLower(filterStr);
            for (auto *obj : allObjects)
            {
                if (obj->MatchesFilter(lowerFilter))
                {
                    m_filteredObjects.push_back(obj);
                }
            }
        }
        else
        {
            m_filteredObjects.assign(allObjects.begin(), allObjects.end());
        }

        // Compute new selection indices and retain new generation objects
        std::vector<uint32_t> indicesToReselect;
        if (preserveSelection && !selectedIds.empty())
        {
            for (size_t i = 0; i < m_filteredObjects.size(); ++i)
            {
                if (selectedIds.contains(m_filteredObjects[i]->GetStableID()))
                {
                    indicesToReselect.push_back(static_cast<uint32_t>(i));
                    m_filteredObjects[i]->Retain(REFCOUNT_DEBUG_ARGS);
                    m_dispatchContext.m_selectedObjects.push_back(m_filteredObjects[i]);
                }
            }
        }

        const uint32_t targetCount = static_cast<uint32_t>(m_filteredObjects.size());
        const uint32_t currentCount = m_listItems ? m_listItems.Size() : 0;
        const int64_t diff = static_cast<int64_t>(targetCount) - static_cast<int64_t>(currentCount);

        m_bIsUpdatingSelection = true;

        bool didResetSource = false;
        if (!forceReset && m_listItems && std::abs(diff) <= 16)
        {
            // Zero-flicker in-place delta update: preserves existing ListViewItem containers
            while (m_listItems.Size() > targetCount)
            {
                m_listItems.RemoveAtEnd();
            }
            while (m_listItems.Size() < targetCount)
            {
                uint32_t nextIdx = m_listItems.Size();
                m_listItems.Append(box_value(nextIdx));
            }
        }
        else
        {
            // Bulk replace when switching views or large filter count jump
            std::vector<IInspectable> boxedIndices;
            boxedIndices.reserve(targetCount);
            for (uint32_t i = 0; i < targetCount; ++i)
            {
                boxedIndices.push_back(box_value(i));
            }
            m_listItems = winrt::single_threaded_observable_vector<IInspectable>(std::move(boxedIndices));
            DataListView().ItemsSource(m_listItems);
            didResetSource = true;
        }

        // Only mutate SelectedItems if the selected row indices actually changed (prevents selection blink on auto-refresh)
        bool selectionChanged = didResetSource;
        if (!selectionChanged)
        {
            std::vector<uint32_t> currentSelected;
            for (auto const &selBox : DataListView().SelectedItems())
            {
                currentSelected.push_back(unbox_value_or<uint32_t>(selBox, UINT32_MAX));
            }
            std::sort(currentSelected.begin(), currentSelected.end());
            selectionChanged = (currentSelected != indicesToReselect);
        }

        if (selectionChanged)
        {
            DataListView().SelectedItems().Clear();
            for (uint32_t idx : indicesToReselect)
            {
                if (idx < m_listItems.Size())
                {
                    DataListView().SelectedItems().Append(m_listItems.GetAt(idx));
                }
            }
        }

        m_bIsUpdatingSelection = false;

        if (!didResetSource)
        {
            RefreshVisibleRowsInPlace();
        }
        SyncListScrollViewer();
        UpdateQuickActionButtons();
        UpdateStatusBar();
    }

    void MainWindow::RefreshVisibleRowsInPlace()
    {
        if (!m_pCurrentController)
            return;

        if (auto itemsPanel = DataListView().ItemsPanelRoot())
        {
            for (auto const &child : itemsPanel.Children())
            {
                if (auto lvi = child.try_as<ListViewItem>())
                {
                    int index = DataListView().IndexFromContainer(lvi);
                    if (index >= 0 && static_cast<size_t>(index) < m_filteredObjects.size())
                    {
                        Grid rowGrid = lvi.ContentTemplateRoot().try_as<Grid>();
                        if (!rowGrid)
                        {
                            rowGrid = lvi.Content().try_as<Grid>();
                        }
                        if (rowGrid)
                        {
                            lvi.Background((index % 2 == 1) ? m_altRowBrush : m_transparentBrush);
                            UpdateRowVisuals(rowGrid, m_filteredObjects[static_cast<size_t>(index)]);
                        }
                    }
                }
            }
        }
    }

    void MainWindow::OnContainerContentChanging(ListViewBase const &, ContainerContentChangingEventArgs const &args)
    {
        if (args.InRecycleQueue() || !m_pCurrentController)
            return;

        auto container = args.ItemContainer();
        if (!container)
            return;

        int itemIndex = args.ItemIndex();
        if (itemIndex < 0 || static_cast<size_t>(itemIndex) >= m_filteredObjects.size())
            return;

        pserv::DataObject *dataObject = m_filteredObjects[static_cast<size_t>(itemIndex)];
        const auto &columns = m_pCurrentController->GetColumns();

        Grid rowGrid = container.ContentTemplateRoot().try_as<Grid>();
        if (!rowGrid)
        {
            rowGrid = Grid();
            container.Content(rowGrid);
        }

        uint32_t rowSchemaId = unbox_value_or<uint32_t>(rowGrid.Tag(), 0);
        if (rowSchemaId != m_currentColumnSchemaId || rowGrid.Children().Size() != columns.size())
        {
            rowGrid.Children().Clear();
            rowGrid.ColumnDefinitions().Clear();
            rowGrid.Tag(box_value(m_currentColumnSchemaId));
            rowGrid.Height(std::max(26.0, m_fontSize + 13.0));

            for (size_t i = 0; i < columns.size(); ++i)
            {
                double colW = (i < m_columnWidths.size()) ? m_columnWidths[i] : 140.0;
                ColumnDefinition colDef;
                colDef.Width(GridLengthHelper::FromPixels(colW));
                rowGrid.ColumnDefinitions().Append(colDef);

                TextBlock cellText;
                cellText.VerticalAlignment(VerticalAlignment::Center);
                cellText.Margin(ThicknessHelper::FromLengths(8, 0, 8, 0));
                cellText.TextTrimming(TextTrimming::CharacterEllipsis);
                cellText.MaxLines(1);
                cellText.FontSize(m_fontSize);

                if (columns[i].GetAlignment() == pserv::ColumnAlignment::Right)
                {
                    cellText.HorizontalAlignment(HorizontalAlignment::Right);
                    cellText.TextAlignment(TextAlignment::Right);
                }
                else
                {
                    cellText.HorizontalAlignment(HorizontalAlignment::Left);
                    cellText.TextAlignment(TextAlignment::Left);
                }

                Grid::SetColumn(cellText, static_cast<int32_t>(i));
                rowGrid.Children().Append(cellText);
            }
        }

        // Alternating row background
        container.Background((itemIndex % 2 == 1) ? m_altRowBrush : m_transparentBrush);

        UpdateRowVisuals(rowGrid, dataObject);
        args.Handled(true);
    }

    void MainWindow::UpdateRowVisuals(Grid const &rowGrid, pserv::DataObject *dataObject)
    {
        if (!rowGrid || !dataObject || !m_pCurrentController)
            return;

        const double desiredHeight = std::max(26.0, m_fontSize + 13.0);
        if (rowGrid.Height() != desiredHeight)
        {
            rowGrid.Height(desiredHeight);
        }

        pserv::VisualState state = m_pCurrentController->GetVisualState(dataObject);
        SolidColorBrush textBrush = m_normalTextBrush;
        if (state == pserv::VisualState::Highlighted)
        {
            textBrush = m_highlightTextBrush;
        }
        else if (state == pserv::VisualState::Disabled)
        {
            textBrush = m_disabledTextBrush;
        }

        auto children = rowGrid.Children();
        auto colDefs = rowGrid.ColumnDefinitions();
        uint32_t count = children.Size();

        for (uint32_t i = 0; i < count; ++i)
        {
            if (i < m_columnWidths.size() && i < colDefs.Size())
            {
                auto colDef = colDefs.GetAt(i);
                if (colDef.Width().Value != m_columnWidths[i])
                {
                    colDef.Width(GridLengthHelper::FromPixels(m_columnWidths[i]));
                }
            }
            if (auto tb = children.GetAt(i).try_as<TextBlock>())
            {
                winrt::hstring newText = winrt::to_hstring(dataObject->GetProperty(static_cast<int>(i)));
                if (tb.Text() != newText)
                {
                    tb.Text(newText);
                }
                if (tb.Foreground() != textBrush)
                {
                    tb.Foreground(textBrush);
                }
                if (tb.FontSize() != m_fontSize)
                {
                    tb.FontSize(m_fontSize);
                }
            }
        }
    }

    void MainWindow::OnListSelectionChanged(IInspectable const &, SelectionChangedEventArgs const &)
    {
        if (m_bIsUpdatingSelection)
            return;

        for (auto *obj : m_dispatchContext.m_selectedObjects)
        {
            obj->Release(REFCOUNT_DEBUG_ARGS);
        }
        m_dispatchContext.m_selectedObjects.clear();

        for (auto const &selectedBox : DataListView().SelectedItems())
        {
            uint32_t idx = unbox_value_or<uint32_t>(selectedBox, UINT32_MAX);
            if (idx < m_filteredObjects.size())
            {
                auto *obj = m_filteredObjects[idx];
                obj->Retain(REFCOUNT_DEBUG_ARGS);
                m_dispatchContext.m_selectedObjects.push_back(obj);
            }
        }

        UpdateQuickActionButtons();
        UpdateStatusBar();
    }

    void MainWindow::UpdateQuickActionButtons()
    {
        bool hasSelection = !m_dispatchContext.m_selectedObjects.empty();
        PropertiesButton().IsEnabled(hasSelection);

        std::vector<const pserv::DataAction *> visibleActions;
        if (m_pCurrentController && hasSelection)
        {
            const pserv::DataObject *firstObj = m_dispatchContext.m_selectedObjects[0];
            auto actions = m_pCurrentController->GetActions(firstObj);

            for (const auto *action : actions)
            {
                if (!action || action->IsSeparator())
                    continue;

                auto vis = action->GetVisibility();
                if ((static_cast<int>(vis) & static_cast<int>(pserv::ActionVisibility::PropertiesDialog)) == 0)
                    continue;

                if (!action->IsAvailableFor(firstObj))
                    continue;

                visibleActions.push_back(action);
            }
        }

        // Skip rebuilding toolbar buttons when the exact same action set is already shown (prevents flicker & preserves tooltips)
        if (visibleActions == m_lastToolbarActions)
            return;

        m_lastToolbarActions = visibleActions;

        auto panelChildren = DynamicActionButtonsPanel().Children();
        panelChildren.Clear();

        for (const auto *action : visibleActions)
        {
            FontIcon icon;
            icon.Glyph(GetActionIconGlyph(action->GetName()));
            icon.FontSize(14);

            Button btn;
            btn.Content(icon);
            btn.Padding(ThicknessHelper::FromLengths(8, 5, 8, 5));
            btn.MinWidth(34);
            btn.Height(32);
            ToolTipService::SetToolTip(btn, box_value(winrt::to_hstring(action->GetName())));

            if (action->IsDestructive() && m_destructiveBrush)
            {
                btn.Foreground(m_destructiveBrush);
            }

            const pserv::DataAction *capturedAction = action;
            btn.Click([this, capturedAction](IInspectable const &, RoutedEventArgs const &)
            {
                ExecuteAction(capturedAction);
            });

            panelChildren.Append(btn);
        }
    }

    void MainWindow::OnListDoubleTapped(IInspectable const &, DoubleTappedRoutedEventArgs const &)
    {
        if (!m_dispatchContext.m_selectedObjects.empty() && m_pCurrentController)
        {
            pserv::theDataPropertiesAction.Execute(m_dispatchContext);
            HandlePostActionContext();
        }
    }

    void MainWindow::OnListRightTapped(IInspectable const &, RightTappedRoutedEventArgs const &e)
    {
        if (!m_pCurrentController)
            return;

        // Identify which row was right-clicked
        DependencyObject curr = e.OriginalSource().try_as<DependencyObject>();
        ListViewItem clickedContainer{nullptr};
        while (curr && curr != DataListView())
        {
            if (auto lvi = curr.try_as<ListViewItem>())
            {
                clickedContainer = lvi;
                break;
            }
            curr = VisualTreeHelper::GetParent(curr);
        }

        if (clickedContainer)
        {
            int clickedIdx = DataListView().IndexFromContainer(clickedContainer);
            if (clickedIdx >= 0 && static_cast<size_t>(clickedIdx) < m_filteredObjects.size())
            {
                pserv::DataObject *clickedObj = m_filteredObjects[static_cast<size_t>(clickedIdx)];
                bool alreadySelected = std::find(
                    m_dispatchContext.m_selectedObjects.begin(),
                    m_dispatchContext.m_selectedObjects.end(),
                    clickedObj) != m_dispatchContext.m_selectedObjects.end();

                if (!alreadySelected)
                {
                    DataListView().SelectedIndex(clickedIdx);
                }
            }
        }

        if (m_dispatchContext.m_selectedObjects.empty())
            return;

        const pserv::DataObject *targetObj = m_dispatchContext.m_selectedObjects[0];
        auto allActions = m_pCurrentController->GetActions(targetObj);
        pserv::AddCommonExportActions(allActions);

        MenuFlyout flyout;
        bool lastWasSeparator = true;

        for (const auto *action : allActions)
        {
            if (!action)
                continue;

            auto vis = action->GetVisibility();
            if ((static_cast<int>(vis) & static_cast<int>(pserv::ActionVisibility::ContextMenu)) == 0)
                continue;

            if (!action->IsAvailableFor(targetObj))
                continue;

            if (action->IsSeparator())
            {
                if (!lastWasSeparator)
                {
                    flyout.Items().Append(MenuFlyoutSeparator());
                    lastWasSeparator = true;
                }
                continue;
            }

            std::string label = action->GetName();
            if (m_dispatchContext.m_selectedObjects.size() > 1)
            {
                label += std::format(" ({} selected)", m_dispatchContext.m_selectedObjects.size());
            }

            MenuFlyoutItem item;
            item.Text(winrt::to_hstring(label));
            FontIcon menuIcon;
            menuIcon.Glyph(GetActionIconGlyph(action->GetName()));
            item.Icon(menuIcon);
            if (action->IsDestructive() && m_destructiveBrush)
            {
                item.Foreground(m_destructiveBrush);
            }

            const pserv::DataAction *capturedAction = action;
            item.Click([this, capturedAction](IInspectable const &, RoutedEventArgs const &)
            {
                ExecuteAction(capturedAction);
            });

            flyout.Items().Append(item);
            lastWasSeparator = false;
        }

        if (flyout.Items().Size() > 0)
        {
            flyout.ShowAt(DataListView(), e.GetPosition(DataListView()));
        }
    }

    void MainWindow::ExecuteAction(const pserv::DataAction *action)
    {
        if (!action || !m_pCurrentController)
            return;

        m_dispatchContext.m_hWnd = m_hWnd;
        m_dispatchContext.m_pController = m_pCurrentController;
        m_dispatchContext.m_bNeedsRefresh = false;
        m_dispatchContext.m_bOpenPropertiesDialog = false;
        m_dispatchContext.m_bShowAddEnvVarDialog = false;

        try
        {
            action->Execute(m_dispatchContext);
        }
        catch (const std::exception &ex)
        {
            spdlog::error("Action '{}' failed: {}", action->GetName(), ex.what());
            ShowErrorDialogAsync(ex.what());
        }

        HandlePostActionContext();
    }

    void MainWindow::HandlePostActionContext()
    {
        if (m_dispatchContext.m_bShowProgressDialog && m_dispatchContext.m_pAsyncOp)
        {
            AsyncProgressBanner().Visibility(Visibility::Visible);
            AsyncProgressMessageText().Text(winrt::to_hstring(m_dispatchContext.m_pAsyncOp->GetProgressMessage()));
            AsyncProgressBar().Value(m_dispatchContext.m_pAsyncOp->GetProgress() * 100.0);
        }

        if (m_dispatchContext.m_bNeedsRefresh && m_pCurrentController)
        {
            m_dispatchContext.m_bNeedsRefresh = false;
            m_pCurrentController->Refresh(false);
            RebuildFilteredList(true);
        }

        if (m_dispatchContext.m_bOpenPropertiesDialog)
        {
            m_dispatchContext.m_bOpenPropertiesDialog = false;
            ShowPropertiesDialogAsync();
        }

        if (m_dispatchContext.m_bShowAddEnvVarDialog)
        {
            m_dispatchContext.m_bShowAddEnvVarDialog = false;
            ShowAddEnvironmentVariableDialogAsync(m_dispatchContext.m_addEnvVarScope);
        }
    }

    void MainWindow::OnAsyncOperationComplete()
    {
        m_dispatchContext.m_bShowProgressDialog = false;
        AsyncProgressBanner().Visibility(Visibility::Collapsed);

        if (m_dispatchContext.m_pAsyncOp)
        {
            auto status = m_dispatchContext.m_pAsyncOp->GetStatus();
            std::string errorMsg = m_dispatchContext.m_pAsyncOp->GetErrorMessage();

            delete m_dispatchContext.m_pAsyncOp;
            m_dispatchContext.m_pAsyncOp = nullptr;

            if (m_pCurrentController)
            {
                m_pCurrentController->Refresh(false);
                RebuildFilteredList(true);
            }

            if (status == pserv::AsyncStatus::Failed && !errorMsg.empty())
            {
                ShowErrorDialogAsync(errorMsg);
            }
        }
    }

    void MainWindow::OnAsyncCancelClick(IInspectable const &, RoutedEventArgs const &)
    {
        if (m_dispatchContext.m_pAsyncOp)
        {
            m_dispatchContext.m_pAsyncOp->RequestCancel();
        }
    }

    void MainWindow::UpdateStatusBar()
    {
        if (!m_pCurrentController)
            return;

        const auto &allObjects = m_pCurrentController->GetDataObjects();
        size_t totalCount = allObjects.GetSize();
        size_t visibleCount = m_filteredObjects.size();
        size_t filteredCount = (totalCount >= visibleCount) ? (totalCount - visibleCount) : 0;
        size_t selectedCount = m_dispatchContext.m_selectedObjects.size();

        size_t highlightedCount = 0;
        size_t disabledCount = 0;
        for (const auto *obj : allObjects)
        {
            auto state = m_pCurrentController->GetVisualState(obj);
            if (state == pserv::VisualState::Highlighted)
                ++highlightedCount;
            else if (state == pserv::VisualState::Disabled)
                ++disabledCount;
        }

        StatusVisibleText().Text(winrt::to_hstring(std::format("{} visible", visibleCount)));
        StatusHighlightedText().Text(winrt::to_hstring(std::format("{} highlighted", highlightedCount)));
        StatusDisabledText().Text(winrt::to_hstring(std::format("{} disabled", disabledCount)));
        StatusFilteredText().Text(winrt::to_hstring(std::format("{} filtered", filteredCount)));
        StatusTotalText().Text(winrt::to_hstring(std::format("{} total", totalCount)));
        StatusSelectedText().Text(winrt::to_hstring(std::format("{} selected", selectedCount)));

        if (pserv::config::theSettings.autoRefresh.enabled.get())
        {
            int32_t intervalMs = pserv::config::theSettings.autoRefresh.intervalMs.get();
            if (m_pCurrentController->SupportsAutoRefresh())
            {
                std::string intervalLabel;
                if (intervalMs == 1000)
                {
                    intervalLabel = "Every 1 second";
                }
                else if (intervalMs > 0 && (intervalMs % 1000) == 0)
                {
                    intervalLabel = std::format("Every {} seconds", intervalMs / 1000);
                }
                else
                {
                    intervalLabel = std::format("Every {} ms", intervalMs);
                }
                StatusAutoRefreshText().Text(winrt::to_hstring(std::format("Auto-refresh: {}", intervalLabel)));
            }
            else
            {
                StatusAutoRefreshText().Text(L"Auto-refresh: Paused (static view)");
            }
        }
        else
        {
            StatusAutoRefreshText().Text(L"Auto-refresh: Off");
        }

        if (m_pCurrentController->IsLoaded())
        {
            auto refreshTime = m_pCurrentController->GetLastRefreshTime();
            auto timeT = std::chrono::system_clock::to_time_t(refreshTime);
            std::tm tm{};
            localtime_s(&tm, &timeT);
            StatusRefreshTimeText().Text(winrt::to_hstring(std::format("Refreshed: {:02d}:{:02d}:{:02d}", tm.tm_hour, tm.tm_min, tm.tm_sec)));
        }
    }

    void MainWindow::OnTimerTick()
    {
        // Update async progress bar if running
        if (m_dispatchContext.m_pAsyncOp && m_dispatchContext.m_bShowProgressDialog)
        {
            AsyncProgressBanner().Visibility(Visibility::Visible);
            AsyncProgressBar().Value(m_dispatchContext.m_pAsyncOp->GetProgress() * 100.0);
            AsyncProgressMessageText().Text(winrt::to_hstring(m_dispatchContext.m_pAsyncOp->GetProgressMessage()));
        }

        if (!ShouldAutoRefresh())
            return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAutoRefreshTime);
        if (elapsed.count() >= pserv::config::theSettings.autoRefresh.intervalMs.get())
        {
            m_lastAutoRefreshTime = now;
            if (m_pCurrentController)
            {
                m_pCurrentController->Refresh(true);
                RebuildFilteredList(true, false);
            }
        }
    }

    bool MainWindow::ShouldAutoRefresh() const
    {
        auto &settings = pserv::config::theSettings.autoRefresh;
        if (!settings.enabled.get() || !m_pCurrentController)
            return false;
        if (!m_pCurrentController->SupportsAutoRefresh())
            return false;
        if (settings.pauseDuringActions.get() && m_dispatchContext.m_pAsyncOp != nullptr)
            return false;
        if (settings.pauseDuringEdits.get() && m_bDialogOpen)
            return false;
        return true;
    }

    // =========================================================================
    // Native WinUI 3 ContentDialogs (Properties, Remote Machine, Add Env Var, Error, About)
    // =========================================================================

    IAsyncAction MainWindow::ShowPropertiesDialogAsync()
    {
        if (m_bDialogOpen || !m_pCurrentController || m_dispatchContext.m_selectedObjects.empty())
            co_return;

        m_bDialogOpen = true;

        // Retain copies of selected objects so they remain alive while the dialog is open
        std::vector<pserv::DataObject *> objects = m_dispatchContext.m_selectedObjects;
        for (auto *obj : objects)
        {
            obj->Retain(REFCOUNT_DEBUG_ARGS);
        }

        // Map: [objectIndex][columnIndex] -> edited string value
        auto editBuffers = std::make_shared<std::map<size_t, std::map<int, std::string>>>();
        const auto &columns = m_pCurrentController->GetColumns();

        auto buildObjectEditor = [&](size_t objIdx, pserv::DataObject *obj) -> UIElement
        {
            ScrollViewer scrollViewer;
            scrollViewer.MaxHeight(460);
            scrollViewer.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);

            StackPanel formPanel;
            formPanel.Spacing(10);
            formPanel.Padding(ThicknessHelper::FromLengths(0, 4, 14, 4));

            for (size_t colIdx = 0; colIdx < columns.size(); ++colIdx)
            {
                const auto &col = columns[colIdx];
                int columnIndex = static_cast<int>(colIdx);
                std::string initialValue = obj->GetProperty(columnIndex);

                Grid fieldGrid;
                ColumnDefinition c0;
                c0.Width(GridLengthHelper::FromPixels(160));
                ColumnDefinition c1;
                c1.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
                fieldGrid.ColumnDefinitions().Append(c0);
                fieldGrid.ColumnDefinitions().Append(c1);

                TextBlock label;
                label.Text(winrt::to_hstring(col.DisplayName));
                label.VerticalAlignment(VerticalAlignment::Center);
                label.FontWeight(col.Editable ? winrt::Windows::UI::Text::FontWeights::SemiBold() : winrt::Windows::UI::Text::FontWeights::Normal());
                label.Opacity(col.Editable ? 1.0 : 0.75);
                Grid::SetColumn(label, 0);
                fieldGrid.Children().Append(label);

                if (col.Editable && col.EditType == pserv::ColumnEditType::Combo)
                {
                    ComboBox combo;
                    combo.HorizontalAlignment(HorizontalAlignment::Stretch);
                    auto options = m_pCurrentController->GetComboOptions(columnIndex);
                    int selectedOption = 0;
                    for (size_t j = 0; j < options.size(); ++j)
                    {
                        combo.Items().Append(box_value(winrt::to_hstring(options[j])));
                        if (options[j] == initialValue)
                        {
                            selectedOption = static_cast<int>(j);
                        }
                    }
                    if (!options.empty())
                    {
                        combo.SelectedIndex(selectedOption);
                    }
                    combo.SelectionChanged([editBuffers, objIdx, columnIndex, options](IInspectable const &s, SelectionChangedEventArgs const &)
                    {
                        auto cb = s.as<ComboBox>();
                        int idx = cb.SelectedIndex();
                        if (idx >= 0 && static_cast<size_t>(idx) < options.size())
                        {
                            (*editBuffers)[objIdx][columnIndex] = options[static_cast<size_t>(idx)];
                        }
                    });
                    Grid::SetColumn(combo, 1);
                    fieldGrid.Children().Append(combo);
                }
                else
                {
                    TextBox tb;
                    tb.Text(winrt::to_hstring(initialValue));
                    tb.IsReadOnly(!col.Editable);
                    tb.HorizontalAlignment(HorizontalAlignment::Stretch);

                    if (col.EditType == pserv::ColumnEditType::TextMultiline || initialValue.find('\n') != std::string::npos)
                    {
                        tb.AcceptsReturn(true);
                        tb.TextWrapping(TextWrapping::Wrap);
                        tb.MinHeight(72);
                    }

                    if (col.Editable)
                    {
                        tb.TextChanged([editBuffers, objIdx, columnIndex](IInspectable const &s, TextChangedEventArgs const &)
                        {
                            auto box = s.as<TextBox>();
                            (*editBuffers)[objIdx][columnIndex] = winrt::to_string(box.Text());
                        });
                    }

                    Grid::SetColumn(tb, 1);
                    fieldGrid.Children().Append(tb);
                }

                formPanel.Children().Append(fieldGrid);
            }

            scrollViewer.Content(formPanel);
            return scrollViewer;
        };

        ContentDialog dialog;
        dialog.XamlRoot(RootGrid().XamlRoot());
        dialog.RequestedTheme(RootGrid().RequestedTheme());

        std::string title = (objects.size() == 1)
            ? std::format("{} Properties — {}", m_pCurrentController->GetItemName(), objects[0]->GetItemName())
            : std::format("{} Properties ({} selected)", m_pCurrentController->GetItemName(), objects.size());

        dialog.Title(box_value(winrt::to_hstring(title)));
        dialog.PrimaryButtonText(L"Apply & OK");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);

        if (objects.size() == 1)
        {
            UIElement content = buildObjectEditor(0, objects[0]);
            if (auto fe = content.try_as<FrameworkElement>())
            {
                fe.MinWidth(520);
            }
            dialog.Content(content);
        }
        else
        {
            Pivot pivot;
            pivot.MinWidth(540);
            for (size_t i = 0; i < std::min<size_t>(objects.size(), 15); ++i)
            {
                PivotItem pItem;
                pItem.Header(box_value(winrt::to_hstring(objects[i]->GetItemName())));
                pItem.Content(buildObjectEditor(i, objects[i]));
                pivot.Items().Append(pItem);
            }
            dialog.Content(pivot);
        }

        auto result = co_await dialog.ShowAsync();
        m_bDialogOpen = false;

        if (result == ContentDialogResult::Primary && !editBuffers->empty())
        {
            bool anyApplied = false;
            for (const auto &[objIdx, colEdits] : *editBuffers)
            {
                if (colEdits.empty() || objIdx >= objects.size())
                    continue;

                pserv::DataObject *obj = objects[objIdx];
                m_pCurrentController->BeginPropertyEdits(obj);
                bool allOk = true;
                for (const auto &[colIdx, newVal] : colEdits)
                {
                    if (!m_pCurrentController->SetPropertyEdit(obj, colIdx, newVal))
                    {
                        allOk = false;
                    }
                }
                if (allOk && m_pCurrentController->CommitPropertyEdits(obj))
                {
                    anyApplied = true;
                }
            }

            if (anyApplied)
            {
                m_pCurrentController->Refresh(false);
                RebuildFilteredList(true);
            }
        }

        for (auto *obj : objects)
        {
            obj->Release(REFCOUNT_DEBUG_ARGS);
        }
    }

    IAsyncAction MainWindow::ShowRemoteMachineDialogAsync()
    {
        if (m_bDialogOpen)
            co_return;
        m_bDialogOpen = true;

        ContentDialog dialog;
        dialog.XamlRoot(RootGrid().XamlRoot());
        dialog.RequestedTheme(RootGrid().RequestedTheme());
        dialog.Title(box_value(L"Connect to Remote Machine"));
        dialog.PrimaryButtonText(L"Connect");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);

        StackPanel panel;
        panel.Spacing(10);
        panel.MinWidth(380);

        TextBlock prompt;
        prompt.Text(L"Enter the hostname or IP address of the remote Windows machine:");
        panel.Children().Append(prompt);

        TextBox machineInput;
        machineInput.PlaceholderText(L"e.g. SERVER01 or 192.168.1.50");
        machineInput.Text(winrt::to_hstring(pserv::config::theSettings.application.serviceMachineName.get()));
        panel.Children().Append(machineInput);

        dialog.Content(panel);

        auto result = co_await dialog.ShowAsync();
        m_bDialogOpen = false;

        if (result == ContentDialogResult::Primary)
        {
            std::string targetMachine = winrt::to_string(machineInput.Text());
            if (!targetMachine.empty())
            {
                pserv::ServiceManager testConn(targetMachine);
                if (!testConn.IsConnected())
                {
                    co_await ShowErrorDialogAsync(std::format(
                        "Failed to connect to '{}'. Ensure the machine is reachable and you have administrative access.",
                        targetMachine));
                    co_return;
                }

                const auto &controllers = m_controllers.GetDataControllers();
                if (!controllers.empty())
                {
                    auto *servicesCtrl = static_cast<pserv::ServicesDataController *>(controllers[0]);
                    pserv::config::theSettings.application.serviceMachineName.set(targetMachine);
                    servicesCtrl->SetMachineName(targetMachine);
                    if (m_pConfigBackend)
                    {
                        pserv::config::theSettings.save(*m_pConfigBackend);
                    }
                    UpdateRemoteModeUI();
                    SwitchToController(servicesCtrl);
                    static_cast<pserv::DataController *>(servicesCtrl)->Refresh(false);
                    RebuildFilteredList(false);
                }
            }
        }
    }

    IAsyncAction MainWindow::ShowAddEnvironmentVariableDialogAsync(int scopeInt)
    {
        if (m_bDialogOpen)
            co_return;
        m_bDialogOpen = true;

        auto scope = static_cast<pserv::EnvironmentVariableScope>(scopeInt);
        const char *scopeStr = (scope == pserv::EnvironmentVariableScope::System) ? "System" : "User";

        ContentDialog dialog;
        dialog.XamlRoot(RootGrid().XamlRoot());
        dialog.RequestedTheme(RootGrid().RequestedTheme());
        dialog.Title(box_value(winrt::to_hstring(std::format("Add {} Environment Variable", scopeStr))));
        dialog.PrimaryButtonText(L"Add");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);

        StackPanel panel;
        panel.Spacing(10);
        panel.MinWidth(420);

        TextBox nameBox;
        nameBox.Header(box_value(L"Variable Name"));
        nameBox.PlaceholderText(L"MY_VARIABLE_NAME");
        panel.Children().Append(nameBox);

        TextBox valueBox;
        valueBox.Header(box_value(L"Variable Value"));
        valueBox.PlaceholderText(L"Value...");
        panel.Children().Append(valueBox);

        dialog.Content(panel);

        auto result = co_await dialog.ShowAsync();
        m_bDialogOpen = false;

        if (result == ContentDialogResult::Primary)
        {
            std::string varName = winrt::to_string(nameBox.Text());
            std::string varValue = winrt::to_string(valueBox.Text());
            if (varName.empty())
            {
                co_await ShowErrorDialogAsync("Variable name cannot be empty.");
                co_return;
            }

            if (pserv::EnvironmentVariableManager::SetEnvironmentVariable(varName, varValue, scope))
            {
                if (m_pCurrentController)
                {
                    m_pCurrentController->Refresh(false);
                    RebuildFilteredList(true);
                }
            }
            else
            {
                co_await ShowErrorDialogAsync(std::format("Failed to create {} environment variable '{}'.", scopeStr, varName));
            }
        }
    }

    IAsyncAction MainWindow::ShowErrorDialogAsync(std::string message)
    {
        if (m_bDialogOpen)
            co_return;
        m_bDialogOpen = true;

        ContentDialog dialog;
        dialog.XamlRoot(RootGrid().XamlRoot());
        dialog.RequestedTheme(RootGrid().RequestedTheme());
        dialog.Title(box_value(L"Operation Error"));
        dialog.Content(box_value(winrt::to_hstring(message)));
        dialog.CloseButtonText(L"OK");
        dialog.DefaultButton(ContentDialogButton::Close);

        co_await dialog.ShowAsync();
        m_bDialogOpen = false;
    }

    IAsyncAction MainWindow::ShowAboutDialogAsync()
    {
        if (m_bDialogOpen)
            co_return;
        m_bDialogOpen = true;

        ContentDialog dialog;
        dialog.XamlRoot(RootGrid().XamlRoot());
        dialog.RequestedTheme(RootGrid().RequestedTheme());
        dialog.Title(box_value(winrt::to_hstring("About pserv6 " PSERV_VERSION_STRING)));
        dialog.CloseButtonText(L"Close");
        dialog.DefaultButton(ContentDialogButton::Close);

        StackPanel panel;
        panel.Spacing(8);

        TextBlock title;
        title.Text(winrt::to_hstring("pserv6 " PSERV_VERSION_STRING " — Windows System Administration Tool"));
        title.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        panel.Children().Append(title);

        TextBlock desc;
        desc.Text(L"Native C++23 / WinUI 3 Fluent Edition.\nManage Services, Devices, Processes, Windows, Uninstaller, Startup Programs, Network Connections, Scheduled Tasks, Modules, and Environment Variables.");
        desc.TextWrapping(TextWrapping::Wrap);
        desc.MaxWidth(420);
        desc.Opacity(0.85);
        panel.Children().Append(desc);

        TextBlock author;
        author.Text(L"Written by Gerson Kurz (https://github.com/gersonkurz/pserv)\nWinUI 3 redesign built with Gemini 4 Neon");
        author.FontSize(12);
        author.Opacity(0.75);
        panel.Children().Append(author);

        dialog.Content(panel);
        co_await dialog.ShowAsync();
        m_bDialogOpen = false;
    }

    // =========================================================================
    // Toolbar, Menu & Keyboard Handlers
    // =========================================================================

    void MainWindow::OnRefreshClick(IInspectable const &, RoutedEventArgs const &)
    {
        if (m_pCurrentController)
        {
            try
            {
                m_pCurrentController->Refresh(false);
                m_pCurrentController->ClearRefreshFlag();
                RebuildFilteredList(true);
            }
            catch (const std::exception &e)
            {
                spdlog::error("Refresh failed: {}", e.what());
            }
        }
    }

    void MainWindow::OnPropertiesClick(IInspectable const &, RoutedEventArgs const &)
    {
        if (!m_dispatchContext.m_selectedObjects.empty())
        {
            ShowPropertiesDialogAsync();
        }
    }

    void MainWindow::OnConnectLocalClick(IInspectable const &, RoutedEventArgs const &)
    {
        const auto &controllers = m_controllers.GetDataControllers();
        if (!controllers.empty())
        {
            auto *servicesCtrl = static_cast<pserv::ServicesDataController *>(controllers[0]);
            pserv::config::theSettings.application.serviceMachineName.set("");
            servicesCtrl->SetMachineName("");
            if (m_pConfigBackend)
            {
                pserv::config::theSettings.save(*m_pConfigBackend);
            }
            UpdateRemoteModeUI();
            if (m_pCurrentController == servicesCtrl)
            {
                static_cast<pserv::DataController *>(servicesCtrl)->Refresh(false);
                RebuildFilteredList(false);
            }
        }
    }

    void MainWindow::OnConnectRemoteClick(IInspectable const &, RoutedEventArgs const &)
    {
        ShowRemoteMachineDialogAsync();
    }

    void MainWindow::OnExitClick(IInspectable const &, RoutedEventArgs const &)
    {
        SaveWindowState();
        SaveCurrentTableState();
        this->Close();
    }

    void MainWindow::OnAutoRefreshToggleClick(IInspectable const &, RoutedEventArgs const &)
    {
        bool newState = !pserv::config::theSettings.autoRefresh.enabled.get();
        pserv::config::theSettings.autoRefresh.enabled.set(newState);
        MenuAutoRefreshToggle().IsChecked(newState);
        if (m_pConfigBackend)
        {
            pserv::config::theSettings.save(*m_pConfigBackend);
        }
        UpdateStatusBar();
    }

    void MainWindow::OnAutoRefreshIntervalClick(IInspectable const &sender, RoutedEventArgs const &)
    {
        if (auto item = sender.try_as<ToggleMenuFlyoutItem>())
        {
            std::string tagStr = winrt::to_string(unbox_value_or<winrt::hstring>(item.Tag(), L"2000"));
            int32_t intervalMs = std::stoi(tagStr);
            pserv::config::theSettings.autoRefresh.intervalMs.set(intervalMs);
            if (m_pConfigBackend)
            {
                pserv::config::theSettings.save(*m_pConfigBackend);
            }

            MenuInterval1s().IsChecked(intervalMs == 1000);
            MenuInterval2s().IsChecked(intervalMs == 2000);
            MenuInterval5s().IsChecked(intervalMs == 5000);
            MenuInterval10s().IsChecked(intervalMs == 10000);
            UpdateStatusBar();
        }
    }

    void MainWindow::ChangeFontSize(double delta)
    {
        double nextSize = std::clamp(m_fontSize + delta, 9.0, 28.0);
        if (nextSize == m_fontSize)
            return;

        m_fontSize = nextSize;
        pserv::config::theSettings.application.fontSizeScaled.set(static_cast<int32_t>(m_fontSize * 100.0));
        if (m_pConfigBackend)
        {
            pserv::config::theSettings.application.save(*m_pConfigBackend);
        }
        BuildColumnHeaders();
        RefreshVisibleRowsInPlace();
    }

    void MainWindow::OnIncreaseFontSizeClick(IInspectable const &, RoutedEventArgs const &)
    {
        ChangeFontSize(+1.0);
    }

    void MainWindow::OnDecreaseFontSizeClick(IInspectable const &, RoutedEventArgs const &)
    {
        ChangeFontSize(-1.0);
    }

    void MainWindow::OnRootPointerWheelChanged(IInspectable const &, PointerRoutedEventArgs const &e)
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
        {
            int32_t delta = e.GetCurrentPoint(RootGrid()).Properties().MouseWheelDelta();
            if (delta > 0)
            {
                ChangeFontSize(+1.0);
            }
            else if (delta < 0)
            {
                ChangeFontSize(-1.0);
            }
            e.Handled(true);
        }
    }

    void MainWindow::OnAboutClick(IInspectable const &, RoutedEventArgs const &)
    {
        ShowAboutDialogAsync();
    }

    void MainWindow::OnDocumentationClick(IInspectable const &, RoutedEventArgs const &)
    {
        ShellExecuteW(nullptr, L"open", L"https://github.com/gersonkurz/pserv", nullptr, nullptr, SW_SHOWNORMAL);
    }

    void MainWindow::OnFilterTextChanged(IInspectable const &, TextChangedEventArgs const &)
    {
        RebuildFilteredList(true);
    }

    void MainWindow::OnRootKeyDown(IInspectable const &, KeyRoutedEventArgs const &e)
    {
        bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        auto key = e.Key();
        int vk = static_cast<int>(key);

        if (ctrlDown && key == VirtualKey::F)
        {
            FilterTextBox().Focus(FocusState::Programmatic);
            FilterTextBox().SelectAll();
            e.Handled(true);
            return;
        }

        if (ctrlDown && (key == VirtualKey::Add || vk == VK_OEM_PLUS))
        {
            ChangeFontSize(+1.0);
            e.Handled(true);
            return;
        }

        if (ctrlDown && (key == VirtualKey::Subtract || vk == VK_OEM_MINUS))
        {
            ChangeFontSize(-1.0);
            e.Handled(true);
            return;
        }

        if (key == VirtualKey::F5)
        {
            OnRefreshClick(nullptr, nullptr);
            e.Handled(true);
            return;
        }

        if (key == VirtualKey::Delete && m_pCurrentController && !m_dispatchContext.m_selectedObjects.empty())
        {
            // Do not hijack Delete while typing inside a TextBox (e.g. FilterTextBox)
            auto focused = FocusManager::GetFocusedElement(RootGrid().XamlRoot());
            if (focused && focused.try_as<TextBox>())
            {
                return;
            }

            const auto *firstSelected = m_dispatchContext.m_selectedObjects[0];
            for (const auto *action : m_pCurrentController->GetActions(firstSelected))
            {
                if (action && (action->GetName() == "Delete" || action->GetName() == "Terminate Process"))
                {
                    ExecuteAction(action);
                    e.Handled(true);
                    return;
                }
            }
        }

        if (ctrlDown)
        {
            int targetIdx = -1;
            if (key >= VirtualKey::Number1 && key <= VirtualKey::Number9)
            {
                targetIdx = static_cast<int>(key) - static_cast<int>(VirtualKey::Number1);
            }
            else if (key == VirtualKey::Number0)
            {
                targetIdx = 9;
            }

            const auto &controllers = m_controllers.GetDataControllers();
            if (targetIdx >= 0 && static_cast<size_t>(targetIdx) < controllers.size())
            {
                auto *targetCtrl = controllers[static_cast<size_t>(targetIdx)];
                std::string machine = pserv::config::theSettings.application.serviceMachineName.get();
                bool isRemote = !machine.empty();
                bool allowed = !isRemote || (targetCtrl->GetControllerName() == pserv::SERVICES_DATA_CONTROLLER_NAME ||
                                             targetCtrl->GetControllerName() == pserv::DEVICES_DATA_CONTROLLER_NAME);
                if (allowed)
                {
                    SwitchToController(targetCtrl);
                    e.Handled(true);
                }
            }
        }
    }
}
