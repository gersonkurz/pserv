#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.g.cpp"
#include "providers.h"
#include <controllers/environment_variables_data_controller.h>
#include <windows_api/environment_variable_manager.h>
#include <core/data_action.h>
#include <actions/common_actions.h>
#include <models/service_info.h>
#include <utils/string_utils.h>
#include <dwmapi.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include "reconcile_rows.h"
#include "keyboard_shortcuts.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;

namespace winrt::pserv6::implementation
{
    namespace
    {
        pserv::DataObject* Object(pserv6::DataRow const& row)
        {
            return get_self<implementation::DataRow>(row)->Object();
        }
        hstring Wide(std::string const& value) { return hstring(pserv::utils::Utf8ToWide(value)); }
        std::string Narrow(hstring const& value) { return pserv::utils::WideToUtf8(value); }
    }

    MainWindow::~MainWindow()
    {
        if (m_timer) m_timer.Stop();
        // Workers own their inputs; destructors cancel and join before releasing them.
        m_refresh.reset();
        m_dispatch.reset();
    }

    bool MainWindow::Busy() const
    {
        return m_refresh || ActionBusy();
    }

    bool MainWindow::ActionBusy() const { return m_dispatch && m_dispatch->m_pAsyncOp; }

    void MainWindow::UpdateRowColors()
    {
        bool dark = Root().ActualTheme() == ElementTheme::Dark;
        Windows::UI::ViewManagement::UISettings settings;
        using Windows::UI::ViewManagement::UIColorType;
        auto normal = dark ? Windows::UI::Color{255, 243, 243, 243} : Windows::UI::Color{255, 26, 26, 26};
        auto disabled = dark ? Windows::UI::Color{255, 145, 145, 145} : Windows::UI::Color{255, 112, 112, 112};
        auto running = settings.GetColorValue(dark ? UIColorType::AccentLight2 : UIColorType::AccentDark2);
        if (Windows::UI::ViewManagement::AccessibilitySettings().HighContrast())
        {
            normal = settings.GetColorValue(UIColorType::Foreground);
            disabled = normal;
            running = normal;
        }
        m_normalBrush = Media::SolidColorBrush(normal);
        m_disabledBrush = Media::SolidColorBrush(disabled);
        m_runningBrush = Media::SolidColorBrush(running);
        if (m_customTheme && !Windows::UI::ViewManagement::AccessibilitySettings().HighContrast())
        {
            auto color = [](pserv::theme::PaletteColor const& c) { return Windows::UI::Color{255,
                static_cast<uint8_t>(c.x * 255), static_cast<uint8_t>(c.y * 255), static_cast<uint8_t>(c.z * 255)}; };
            m_normalBrush = Media::SolidColorBrush(color(m_themes.Current().base[5]));
            m_disabledBrush = Media::SolidColorBrush(color(m_themes.Current().base[4]));
            m_runningBrush = Media::SolidColorBrush(color(m_themes.Current().base[0x0B]));
        }
        if (!m_controller) return;
        RefreshRealizedRows();
    }

    void MainWindow::ApplyZoom()
    {
        double scale = m_fontSize / 13.0;
        UiScale().ScaleX(scale);
        UiScale().ScaleY(scale);
        if (Viewport().ActualWidth() > 0) Root().Width(Viewport().ActualWidth() / scale);
        if (Viewport().ActualHeight() > 0) Root().Height(Viewport().ActualHeight() / scale);
    }

    void MainWindow::ViewportSizeChanged(IInspectable const&, SizeChangedEventArgs const&)
    {
        ApplyZoom();
    }

    void MainWindow::WheelChanged(IInspectable const&, Input::PointerRoutedEventArgs const& e)
    {
        if ((e.KeyModifiers() & Windows::System::VirtualKeyModifiers::Control) == Windows::System::VirtualKeyModifiers::None || m_dialogOpen)
            return;
        auto properties = e.GetCurrentPoint(Root()).Properties();
        if (properties.IsHorizontalMouseWheel()) return;
        m_fontSize = std::clamp(m_fontSize + properties.MouseWheelDelta() / 120.0, 8.0, 32.0);
        ApplyZoom();
        e.Handled(true);
    }

    void MainWindow::OnLoaded(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loaded) return;
        m_loaded = true;
        check_hresult(this->try_as<::IWindowNative>()->get_WindowHandle(&m_hWnd));
        auto updateTitleBar = [weak = get_weak()](auto const&, auto const&)
        {
            if (auto self = weak.get())
            {
                BOOL dark = self->Root().ActualTheme() == ElementTheme::Dark;
                DwmSetWindowAttribute(self->m_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
                self->UpdateRowColors();
            }
        };
        Root().ActualThemeChanged(updateTitleBar);
        updateTitleBar(Root(), nullptr);
        ApplyZoom();
        m_items = single_threaded_observable_vector<IInspectable>();
        Rows().ItemsSource(m_items);
        Root().AddHandler(UIElement::PointerWheelChangedEvent(),
            box_value(Input::PointerEventHandler{this, &MainWindow::WheelChanged}), true);
        Rows().AddHandler(UIElement::PointerPressedEvent(), box_value(Input::PointerEventHandler{
            [weak = get_weak()](auto const&, Input::PointerRoutedEventArgs const& e)
            {
                if (auto self = weak.get())
                {
                    auto point = e.GetCurrentPoint(self->Rows());
                    if (!point.Properties().IsLeftButtonPressed() ||
                        (GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_SHIFT) & 0x8000)) return;
                    auto node = e.OriginalSource().try_as<DependencyObject>();
                    while (node && !node.try_as<ListViewItem>()) node = Media::VisualTreeHelper::GetParent(node);
                    if (auto item = node.try_as<ListViewItem>()) self->Rows().SelectedItem(item.Content());
                }
            }}), true);
        AppWindow().Resize({1360, 860});
        wchar_t name[MAX_COMPUTERNAME_LENGTH + 1]{};
        DWORD length = static_cast<DWORD>(std::size(name));
        m_localMachine = GetComputerNameW(name, &length) ? name : L"This computer";
        wil::unique_handle token;
        TOKEN_ELEVATION elevation{};
        DWORD returned{};
        m_elevated = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put()) &&
            GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &returned) && elevation.TokenIsElevated;
        UpdateMachineLabel();
        ElevateButton().Visibility(m_elevated ? Visibility::Collapsed : Visibility::Visible);
        SetButtonContent(ElevateButton(), L"Elevate", L"\uEA18");
        SetButtonContent(PropertiesButton(), L"Properties", L"\uE946");
        SetButtonContent(AddVariableButton(), L"Add variable", L"\uE710");
        SetButtonContent(RefreshButton(), L"Refresh now (F5)", L"\uE72C");
        if (auto icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1)))
        {
            SendMessageW(m_hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
            SendMessageW(m_hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
        }
        LoadAppearance();
        BuildNavigation();
        m_timer = DispatcherQueue().CreateTimer();
        m_timer.Interval(std::chrono::milliseconds(100));
        m_timer.Tick([weak = get_weak()](auto const&, auto const&)
        {
            if (auto self = weak.get()) self->Tick();
        });
        Closed([weak = get_weak()](auto const&, auto const&)
        {
            if (auto self = weak.get())
            {
                self->SaveAppearance();
                self->m_closed = true;
                self->m_timer.Stop();
                if (self->m_refresh) self->m_refresh->RequestCancel();
                if (self->m_dispatch && self->m_dispatch->m_pAsyncOp) self->m_dispatch->m_pAsyncOp->RequestCancel();
            }
        });
        Root().KeyDown([weak = get_weak()](auto const&, Input::KeyRoutedEventArgs const& e)
        {
            auto self = weak.get();
            if (!self || e.Handled()) return;
            auto focused = Input::FocusManager::GetFocusedElement(self->Root().XamlRoot());
            bool textInput = focused && (focused.try_as<TextBox>() || focused.try_as<RichEditBox>() || focused.try_as<PasswordBox>());
            auto node = focused.try_as<DependencyObject>();
            while (node && node != self->Rows()) node = Media::VisualTreeHelper::GetParent(node);
            using pserv::winui::Shortcut;
            auto shortcut = pserv::winui::DecodeShortcut(static_cast<int>(e.Key()),
                (GetKeyState(VK_CONTROL) & 0x8000) != 0, (GetKeyState(VK_SHIFT) & 0x8000) != 0,
                (GetKeyState(VK_MENU) & 0x8000) != 0, textInput, node != nullptr, self->m_dialogOpen);
            switch (shortcut.command)
            {
            case Shortcut::Refresh: self->Refresh(); break;
            case Shortcut::Search:
                self->SearchBox().Focus(FocusState::Keyboard);
                self->SearchBox().SelectAll();
                break;
            case Shortcut::ToggleRefresh:
                if (self->AutoRefresh().IsEnabled()) self->AutoRefresh().IsOn(!self->AutoRefresh().IsOn());
                break;
            case Shortcut::Provider:
                self->SelectProvider(shortcut.provider);
                // A remote/busy view may reject the change; keep navigation consistent.
                if (self->m_activeProvider == shortcut.provider)
                    self->Navigation().SelectedItem(self->Navigation().MenuItems().GetAt(shortcut.provider));
                break;
            case Shortcut::ZoomIn: case Shortcut::ZoomOut: case Shortcut::ResetZoom:
                self->m_fontSize = shortcut.command == Shortcut::ResetZoom ? 13 :
                    std::clamp(self->m_fontSize + (shortcut.command == Shortcut::ZoomIn ? 1 : -1), 8.0, 32.0);
                self->ApplyZoom();
                break;
            case Shortcut::DeleteSelection:
                for (auto action : self->AvailableActions())
                    if (action->GetName() == "Delete" || action->GetName() == "Terminate Process")
                    {
                        self->ExecuteAction(action->GetName()); // Existing availability and confirmation path.
                        break;
                    }
                break;
            default: return;
            }
            e.Handled(true);
        });
        m_timer.Start();
        SelectProvider(m_startProvider);
        Navigation().SelectedItem(Navigation().MenuItems().GetAt(m_startProvider));
    }

    void MainWindow::RowsLoaded(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_rowScroll) return;
        std::function<ScrollViewer(DependencyObject)> find = [&](DependencyObject node) -> ScrollViewer
        {
            if (auto scroll = node.try_as<ScrollViewer>()) return scroll;
            for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(node); ++i)
                if (auto scroll = find(Media::VisualTreeHelper::GetChild(node, i))) return scroll;
            return nullptr;
        };
        m_rowScroll = find(Rows());
        if (m_rowScroll)
        {
            m_rowScroll.HorizontalContentAlignment(HorizontalAlignment::Left);
            m_rowScroll.ViewChanged([weak = get_weak()](auto const& sender, auto const&)
            {
                if (auto self = weak.get()) self->HeaderScroll().ChangeView(sender.template as<ScrollViewer>().HorizontalOffset(), nullptr, nullptr, true);
            });
        }
    }

    void MainWindow::BuildNavigation()
    {
        auto const& providers = pserv::winui::Providers();
        for (int i = 0; i < static_cast<int>(providers.size()); ++i)
        {
            NavigationViewItem item;
            item.Content(box_value(hstring(providers[i].title)));
            item.Tag(box_value(i));
            FontIcon icon;
            icon.Glyph(providers[i].glyph);
            item.Icon(icon);
            Navigation().MenuItems().Append(item);
        }
    }

    void MainWindow::ProviderChanged(NavigationView const&, NavigationViewSelectionChangedEventArgs const& e)
    {
        if (m_loaded && !m_switching)
            if (auto item = e.SelectedItem().try_as<NavigationViewItem>()) SelectProvider(unbox_value<int>(item.Tag()));
    }

    void MainWindow::SelectProvider(int index)
    {
        auto const& providers = pserv::winui::Providers();
        if (index < 0 || index >= static_cast<int>(providers.size()) || index == m_activeProvider || ActionBusy() || m_dialogOpen || (!m_machineName.empty() && !providers[index].supportsRemote)) return;
        if (m_activeProvider >= 0)
            SaveCurrentView();
        m_switching = true;
        m_rebinding = true;
        m_activeProvider = index;
        if (m_refresh) m_discardRefresh = true;
        Rows().SelectedItems().Clear();
        m_items.Clear();
        m_allRows.clear();
        m_disabledCount = m_highlightedCount = 0;
        m_visibleRows.clear();
        auto const& provider = providers[index];
        m_controller = CreateController(index);
        auto const& metadata = m_controller->GetColumns();
        m_columns.clear();
        for (int column : provider.preferredColumns)
            if (column >= 0 && column < static_cast<int>(metadata.size())) m_columns.push_back(column);
        for (int column = 0; column < static_cast<int>(metadata.size()); ++column)
            if (std::ranges::find(m_columns, column) == m_columns.end()) m_columns.push_back(column);
        if (auto saved = m_viewSettings.find(index); saved != m_viewSettings.end() && !saved->second.columns.empty())
        {
            std::vector<int> valid;
            for (int column : saved->second.columns)
                if (column >= 0 && column < static_cast<int>(metadata.size()) && std::ranges::find(valid, column) == valid.end()) valid.push_back(column);
            if (!valid.empty()) m_columns = std::move(valid);
        }
        m_widths.clear();
        for (int column : m_columns)
        {
            auto name = pserv::utils::ToLower(metadata[column].DisplayName);
            double width = metadata[column].DataType == pserv::ColumnDataType::String ? 180 : 110;
            if (name.find("name") != std::string::npos) width = 240;
            if (name.find("path") != std::string::npos || name.find("command") != std::string::npos ||
                name.find("description") != std::string::npos || name == "value" || name == "title") width = 360;
            m_widths.push_back(width);
        }
        auto settings = m_viewSettings.find(index);
        auto saved = settings == m_viewSettings.end() ? ViewSettings{} : settings->second;
        if (saved.widths.size() == m_widths.size()) m_widths = saved.widths;
        m_sortColumn = std::clamp(saved.sortColumn, 0, static_cast<int>(m_columns.size()) - 1);
        m_ascending = saved.ascending;
        SearchBox().Text(saved.filter);
        SearchBox().PlaceholderText(std::wstring(L"Search ") + provider.title);
        Automation::AutomationProperties::SetName(SearchBox(), SearchBox().PlaceholderText());
        Title(std::wstring(L"pserv6 — ") + provider.title + (m_machineName.empty() ? L"" : L" — " + std::wstring(Wide(m_machineName))));
        AutoRefresh().IsEnabled(m_controller->SupportsAutoRefresh());
        m_refreshPreference.SupportsRefresh(m_controller->SupportsAutoRefresh());
        AutoRefresh().IsOn(m_refreshPreference.IsOn());
        AutoRefresh().OffContent(box_value(m_controller->SupportsAutoRefresh() ? L"Paused" : L"Auto-refresh unavailable"));
        ToolTipService::SetToolTip(AutoRefresh(), box_value(m_controller->SupportsAutoRefresh()
            ? L"Pause or resume automatic refresh across all supported views (Ctrl+R)."
            : L"This view supports manual refresh only (F5). Your global automatic-refresh preference is preserved."));
        AddVariableButton().Visibility(provider.supportsAddVariable ? Visibility::Visible : Visibility::Collapsed);
        PrimaryActions().Children().Clear();
        for (auto const& name : provider.toolbarActions)
        {
            Button button;
            auto glyph = name == "Start" || name == "Run Now" ? L"\uE768" :
                name == "Stop" || name == "Terminate Process" ? L"\uE71A" :
                name == "Restart" ? L"\uE777" : name == "Enable" ? L"\uE73E" :
                name == "Disable" ? L"\uE711" : name == "Uninstall" ? L"\uE74D" :
                name == "Bring To Front" ? L"\uE8A7" : // OpenInNewWindow: window with arrow.
                name == "Show" ? L"\uE890" : // View: eye.
                name == "Hide" ? L"\uED1A" : // Hide: crossed-out eye.
                L"\uE8A7";
            SetButtonContent(button, Wide(name), glyph);
            button.Tag(box_value(Wide(name)));
            button.Click({this, &MainWindow::ActionClicked});
            PrimaryActions().Children().Append(button);
        }
        BuildHeaders();
        ApplyFont();
        if (m_rowScroll) m_rowScroll.ChangeView(0.0, 0.0, nullptr, true);
        HeaderScroll().ChangeView(0.0, nullptr, nullptr, true);
        Notice().IsOpen(false);
        RefreshText().Text(L"Loading…");
        m_rebinding = false;
        m_switching = false;
        UpdateActions();
        Refresh();
    }

    void MainWindow::Refresh()
    {
        if (Busy() || m_dialogOpen || m_closed || m_activeProvider < 0) return;
        m_refreshStarted = std::chrono::steady_clock::now();
        m_refreshProvider = m_activeProvider;
        m_pendingController = CreateController(m_activeProvider);
        m_refresh = std::make_unique<pserv::AsyncOperation>();
        m_refresh->Start(nullptr, [controller = m_pendingController](pserv::AsyncOperation* op)
        {
            controller->Refresh();
            if (!controller->IsLoaded()) throw std::runtime_error("Enumeration failed. See the diagnostic log.");
            return !op->IsCancelRequested();
        });
        // A periodic read must not resize the table or flash its action buttons.
        if (!m_controller->IsLoaded()) RefreshText().Text(L"Loading…");
    }

    void MainWindow::Tick()
    {
        if (m_closed) return;
        // Keep menu targets and their available actions stable until the menu closes.
        bool actionsMenuOpen = (m_rowActionsMenu && m_rowActionsMenu.IsOpen()) || ActionsMenu().IsOpen();
        if (m_refresh && !m_dialogOpen && !actionsMenuOpen && !ActionBusy() && m_refresh->GetStatus() != pserv::AsyncStatus::Running)
        {
            m_refresh->Wait();
            bool discarded = std::exchange(m_discardRefresh, false) || m_refreshProvider != m_activeProvider;
            if (!discarded && m_refresh->GetStatus() == pserv::AsyncStatus::Completed)
            {
                m_controller = std::move(m_pendingController);
                RebuildRows();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_refreshStarted).count();
                SYSTEMTIME now{};
                GetLocalTime(&now);
                RefreshText().Text(std::format(L"Last refresh {:02}:{:02}:{:02} · took {} ms", now.wHour, now.wMinute, now.wSecond, elapsed));
            }
            else if (!discarded)
            {
                ShowNotice(Wide(m_refresh->GetErrorMessage()), InfoBarSeverity::Error);
                RefreshText().Text(L"Refresh failed");
            }
            m_refresh.reset();
            m_pendingController.reset();
            m_lastRefresh = std::chrono::steady_clock::now();
            UpdateActions();
            if (discarded) Refresh();
        }
        if (m_dispatch && m_dispatch->m_pAsyncOp)
        {
            auto op = m_dispatch->m_pAsyncOp;
            Notice().Message(Wide(op->GetProgressMessage()));
            if (op->GetStatus() != pserv::AsyncStatus::Running)
            {
                op->Wait();
                auto status = op->GetStatus();
                auto message = status == pserv::AsyncStatus::Failed ? op->GetErrorMessage() : op->GetProgressMessage();
                if (status == pserv::AsyncStatus::Cancelled) message = "Operation cancelled. Changes already completed are retained.";
                ShowNotice(Wide(message), status == pserv::AsyncStatus::Completed ? InfoBarSeverity::Success : InfoBarSeverity::Warning);
                m_dispatch.reset();
                CancelButton().Visibility(Visibility::Collapsed);
                Progress().Visibility(Visibility::Collapsed);
                auto pendingMachine = std::exchange(m_pendingMachine, std::nullopt);
                if (pendingMachine && status == pserv::AsyncStatus::Completed) SetMachine(*pendingMachine);
                else Refresh();
            }
        }
        if (!Busy() && !m_dialogOpen && !actionsMenuOpen && AutoRefresh().IsOn() && m_controller && m_controller->SupportsAutoRefresh() &&
            std::chrono::steady_clock::now() - m_lastRefresh >= std::chrono::milliseconds(m_refreshInterval)) Refresh();
    }

    void MainWindow::RebuildRows()
    {
        m_controller->Sort(m_columns[m_sortColumn], m_ascending);
        std::unordered_map<std::string, pserv6::DataRow> existing;
        for (auto const& row : m_allRows) existing.emplace(Object(row)->GetStableID(), row);
        std::vector<pserv6::DataRow> next;
        for (auto object : m_controller->GetDataObjects())
        {
            auto found = existing.find(object->GetStableID());
            auto row = found == existing.end() ? make<implementation::DataRow>() : found->second;
            get_self<implementation::DataRow>(row)->SetObject(object);
            next.push_back(row);
        }
        m_allRows = std::move(next);
        UpdateCounts();
        ApplyFilter();
    }

    void MainWindow::RenderRow(Grid const& grid, pserv6::DataRow const& row)
    {
        if (!m_controller || !row) return;
        // Only realized containers own cell visuals. The retained row is just a stable identity.
        if (grid.Children().Size() != m_columns.size())
        {
            grid.Children().Clear();
            grid.ColumnDefinitions().Clear();
            for (uint32_t i = 0; i < m_columns.size(); ++i)
            {
                grid.ColumnDefinitions().Append(ColumnDefinition{});
                TextBlock text;
                text.Margin({10, 0, 10, 0});
                text.VerticalAlignment(VerticalAlignment::Center);
                text.TextTrimming(TextTrimming::CharacterEllipsis);
                text.FontSize(13);
                Grid::SetColumn(text, i);
                grid.Children().Append(text);
            }
        }
        auto object = Object(row);
        auto state = m_controller->GetVisualState(object);
        auto brush = state == pserv::VisualState::Disabled ? m_disabledBrush :
            state == pserv::VisualState::Highlighted ? m_runningBrush : m_normalBrush;
        auto const& metadata = m_controller->GetColumns();
        for (uint32_t i = 0; i < m_columns.size(); ++i)
        {
            auto definition = grid.ColumnDefinitions().GetAt(i);
            if (definition.Width().Value != m_widths[i] || definition.Width().GridUnitType != GridUnitType::Pixel)
                definition.Width(GridLengthHelper::FromPixels(m_widths[i]));
            auto text = grid.Children().GetAt(i).as<TextBlock>();
            auto value = Wide(object->GetProperty(m_columns[i]));
            if (text.Text() != value) text.Text(value);
            if (text.Foreground() != brush) text.Foreground(brush);
            if (text.FontFamily().Source() != m_fontFamily) text.FontFamily(Media::FontFamily(m_fontFamily));
            text.FontWeight({m_fontWeight});
            text.FontStyle(m_fontItalic ? Windows::UI::Text::FontStyle::Italic : Windows::UI::Text::FontStyle::Normal);
            text.TextAlignment(metadata[m_columns[i]].GetAlignment() == pserv::ColumnAlignment::Right ? TextAlignment::Right : TextAlignment::Left);
        }
    }

    void MainWindow::RowContentChanging(ListViewBase const&, ContainerContentChangingEventArgs const& args)
    {
        if (args.InRecycleQueue()) return;
        auto container = args.ItemContainer();
        if (!container.ContextFlyout())
        {
            MenuFlyout menu;
            menu.Opening([weak = get_weak(), target = make_weak(container)](auto const& sender, auto const&)
            {
                auto self = weak.get();
                auto item = target.get();
                if (!self || !item || self->m_closed) return;
                auto menu = sender.template as<MenuFlyout>();
                self->m_rowActionsMenu = menu;
                // Resolve the current item at opening: containers are recycled, and
                // right-click need not have updated ListView.SelectedItems yet.
                auto row = self->Rows().ItemFromContainer(item).template try_as<pserv6::DataRow>();
                if (!row) { menu.Items().Clear(); return; }
                uint32_t selectedIndex;
                auto selected = self->Rows().SelectedItems();
                if (!selected.IndexOf(row, selectedIndex))
                {
                    selected.Clear();
                    selected.Append(row);
                }
                self->FillActions(menu);
            });
            container.ContextFlyout(menu);
        }
        auto row = args.Item().try_as<pserv6::DataRow>();
        auto grid = args.ItemContainer().ContentTemplateRoot().try_as<Grid>();
        if (row && grid) { RenderRow(grid, row); args.Handled(true); }
    }

    void MainWindow::RefreshRealizedRows()
    {
        if (auto panel = Rows().ItemsPanelRoot().try_as<ItemsStackPanel>())
            for (int i = std::max(0, panel.FirstCacheIndex()); i <= panel.LastCacheIndex() && i < static_cast<int>(m_items.Size()); ++i)
                if (auto item = Rows().ContainerFromIndex(i).try_as<ListViewItem>())
                    if (auto grid = item.ContentTemplateRoot().try_as<Grid>())
                        RenderRow(grid, m_items.GetAt(i).as<pserv6::DataRow>());
    }

    std::vector<pserv6::DataRow> MainWindow::SelectedRows()
    {
        std::vector<pserv6::DataRow> selected;
        for (auto const& item : Rows().SelectedItems()) selected.push_back(item.as<pserv6::DataRow>());
        return selected;
    }

    void MainWindow::ApplyFilter()
    {
        auto horizontal = m_rowScroll ? m_rowScroll.HorizontalOffset() : 0.0;
        auto vertical = m_rowScroll ? m_rowScroll.VerticalOffset() : 0.0;
        std::unordered_set<std::string> selected;
        for (auto const& row : SelectedRows()) selected.insert(Object(row)->GetStableID());
        auto filter = pserv::utils::ToLower(Narrow(SearchBox().Text()));
        std::vector<IInspectable> visible;
        m_visibleRows.clear();
        for (auto const& row : m_allRows)
        {
            if (Object(row)->MatchesFilter(filter))
            {
                visible.push_back(row);
                m_visibleRows.push_back(row);
            }
        }
        m_rebinding = true;
        bool structureChanged = pserv::winui::ReconcileRows(m_items, visible, [](auto const& item)
        {
            return Object(item.template as<pserv6::DataRow>())->GetStableID();
        });
        if (structureChanged) Rows().SelectedItems().Clear();
        for (auto const& row : m_visibleRows)
        {
            uint32_t index;
            if (selected.contains(Object(row)->GetStableID()) && !Rows().SelectedItems().IndexOf(row, index))
                Rows().SelectedItems().Append(row);
        }
        m_rebinding = false;
        // Collection reconciliation and selection changes must not bring the
        // focused/selected row back into view during periodic refresh.
        if (structureChanged) Rows().UpdateLayout();
        RefreshRealizedRows();
        if (m_rowScroll) m_rowScroll.ChangeView(horizontal, vertical, nullptr, true);
        UpdateActions();
    }

    void MainWindow::ApplyWidths(Grid const& grid)
    {
        if (!grid || grid.ColumnDefinitions().Size() != std::size(m_widths)) return;
        for (uint32_t i = 0; i < std::size(m_widths); ++i)
            grid.ColumnDefinitions().GetAt(i).Width({m_widths[i], GridUnitType::Pixel});
    }

    void MainWindow::BuildHeaders()
    {
        Headers().Children().Clear();
        Headers().ColumnDefinitions().Clear();
        double width = 0;
        for (int i = 0; i < static_cast<int>(m_columns.size()); ++i)
        {
            width += m_widths[i];
            ColumnDefinition column;
            column.Width({m_widths[i], GridUnitType::Pixel});
            Headers().ColumnDefinitions().Append(column);
            Button header;
            header.Content(box_value(std::wstring(Wide(m_controller->GetColumns()[m_columns[i]].DisplayName)) + (i == m_sortColumn ? (m_ascending ? L"  ↑" : L"  ↓") : L"")));
            header.HorizontalAlignment(HorizontalAlignment::Stretch);
            header.HorizontalContentAlignment(HorizontalAlignment::Left);
            header.BorderThickness({0});
            header.Background(Media::SolidColorBrush(Windows::UI::Color{0, 0, 0, 0}));
            header.FontSize(12);
            header.Click([weak = get_weak(), i](auto const&, auto const&)
            {
                if (auto self = weak.get(); self && self->m_controller && !self->m_dialogOpen)
                {
                    self->m_ascending = self->m_sortColumn == i ? !self->m_ascending : true;
                    self->m_sortColumn = i;
                    self->BuildHeaders();
                    self->RebuildRows();
                }
            });
            Grid::SetColumn(header, i);
            Headers().Children().Append(header);
            Controls::Primitives::Thumb handle;
            handle.Width(5);
            handle.HorizontalAlignment(HorizontalAlignment::Right);
            handle.DragDelta([weak = get_weak(), i](auto const&, Controls::Primitives::DragDeltaEventArgs const& e)
            {
                if (auto self = weak.get())
                {
                    self->m_widths[i] = std::clamp(self->m_widths[i] + e.HorizontalChange(), 60.0, 900.0);
                    self->ApplyWidths(self->Headers());
                    double total = 0;
                    for (auto w : self->m_widths) total += w;
                    self->Headers().Width(total);
                    self->RefreshRealizedRows();
                }
            });
            Grid::SetColumn(handle, i);
            Headers().Children().Append(handle);
        }
        Headers().Width(width);
        ApplyFontTo(Headers());
    }

    std::vector<const pserv::DataAction*> MainWindow::AvailableActions()
    {
        auto selected = SelectedRows();
        if (!m_controller || selected.empty() || ActionBusy() || m_dialogOpen) return {};
        auto actions = m_controller->GetActions(Object(selected.front()));
        pserv::AddCommonExportActions(actions);
        for (auto const& row : selected)
        {
            auto object = Object(row);
            auto available = m_controller->GetActions(object);
            pserv::AddCommonExportActions(available);
            std::erase_if(actions, [&](auto action)
            {
                if (action->IsSeparator()) return false;
                auto const& name = action->GetName();
                if (name == "Add User Variable" || name == "Add System Variable") return true;
                if (!m_machineName.empty() && (name == "Open in Registry Editor" || name == "Open in Explorer" ||
                    name == "Open Terminal Here" || name == "Delete Registry Key")) return true;
                return !action->IsAvailableFor(object) || std::ranges::find(available, action) == available.end();
            });
        }
        return actions;
    }

    void MainWindow::UpdateActions()
    {
        auto selected = SelectedRows();
        auto actions = AvailableActions();
        auto has = [&](std::string_view name) { return std::ranges::any_of(actions, [&](auto a) { return a->GetName() == name; }); };
        for (auto const& element : PrimaryActions().Children())
            if (auto button = element.try_as<Button>())
            {
                auto name = Narrow(unbox_value<hstring>(button.Tag()));
                button.IsEnabled(std::ranges::any_of(actions, [&](auto action)
                {
                    return action->GetName() == name &&
                        (static_cast<int>(action->GetVisibility()) & static_cast<int>(pserv::ActionVisibility::PropertiesDialog)) != 0;
                }));
            }
        PropertiesButton().IsEnabled(selected.size() == 1 && !ActionBusy() && !m_dialogOpen);
        MoreButton().IsEnabled(!actions.empty());
        AddVariableButton().IsEnabled(!ActionBusy() && !m_dialogOpen);
        Navigation().IsEnabled(!ActionBusy() && !m_dialogOpen);
        LocalMenuItem().IsEnabled(!Busy() && !m_dialogOpen);
        RemoteMenuItem().IsEnabled(!Busy() && !m_dialogOpen);
        ElevateButton().IsEnabled(!Busy() && !m_dialogOpen);
        CopyTextMenuItem().IsEnabled(has("Copy as Plain Text"));
        CopyJsonMenuItem().IsEnabled(has("Copy as JSON"));
        CountText().Text(std::format(L"{} of {}  ·  {} selected  ·  {} highlighted  ·  {} {}",
            m_visibleRows.size(), m_allRows.size(), selected.size(), m_highlightedCount, m_disabledCount,
            m_activeProvider >= 0 && pserv::winui::Providers().at(m_activeProvider).labelsDisabledState ? L"disabled" : L"dimmed"));
    }

    void MainWindow::FillActions(MenuFlyout const& menu)
    {
        menu.Items().Clear();
        for (auto action : AvailableActions())
        {
            if ((static_cast<int>(action->GetVisibility()) & static_cast<int>(pserv::ActionVisibility::ContextMenu)) == 0) continue;
            if (action->IsSeparator())
            {
                if (menu.Items().Size() && !menu.Items().GetAt(menu.Items().Size() - 1).try_as<MenuFlyoutSeparator>()) menu.Items().Append(MenuFlyoutSeparator());
                continue;
            }
            MenuFlyoutItem item;
            item.Text(Wide(action->GetName()));
            item.Click([weak = get_weak(), name = action->GetName()](auto const&, auto const&)
            {
                if (auto self = weak.get()) self->ExecuteAction(name);
            });
            menu.Items().Append(item);
        }
        if (SelectedRows().size() == 1 && !ActionBusy())
        {
            menu.Items().Append(MenuFlyoutSeparator());
            MenuFlyoutItem item;
            item.Text(L"Properties");
            item.Click([weak = get_weak()](auto const&, auto const&) { if (auto self = weak.get()) self->ShowProperties(); });
            menu.Items().Append(item);
        }
    }

    fire_and_forget MainWindow::ExecuteAction(std::string name)
    {
        auto lifetime = get_strong();
        bool needsRefresh = false;
        auto actions = AvailableActions();
        auto found = std::ranges::find_if(actions, [&](auto a) { return a->GetName() == name; });
        if (found == actions.end()) co_return;
        auto action = *found;
        m_dispatch = std::make_unique<pserv::DataActionDispatchContext>();
        m_dispatch->m_hWnd = m_hWnd;
        m_dispatch->m_pController = m_controller.get();
        for (auto const& row : SelectedRows())
        {
            Object(row)->Retain(REFCOUNT_DEBUG_ARGS);
            m_dispatch->m_selectedObjects.push_back(Object(row));
        }
        // A modal confirmation freezes refresh so the selected snapshot remains valid.
        m_dialogOpen = true;
        UpdateActions();
        try
        {
            if (action->RequiresConfirmation())
            {
                ContentDialog confirm;
                confirm.XamlRoot(Root().XamlRoot());
                confirm.Title(box_value(Wide(name)));
                confirm.Content(box_value(std::format(L"Apply this action to {} selected item(s)?", m_dispatch->m_selectedObjects.size())));
                confirm.PrimaryButtonText(L"Continue");
                confirm.CloseButtonText(L"Cancel");
                if (co_await confirm.ShowAsync() != ContentDialogResult::Primary)
                {
                    m_dispatch.reset();
                    m_dialogOpen = false;
                    if (!m_closed) UpdateActions();
                    co_return;
                }
            }
            if (!m_closed)
            {
                m_discardRefresh = m_refresh != nullptr;
                action->Execute(*m_dispatch);
            }
            if (m_dispatch->m_pAsyncOp)
            {
                ShowNotice(L"Working…", InfoBarSeverity::Informational);
                Progress().IsIndeterminate(true);
                Progress().Visibility(Visibility::Visible);
                CancelButton().Visibility(Visibility::Visible);
            }
            else { needsRefresh = m_dispatch->m_bNeedsRefresh; m_dispatch.reset(); }
        }
        catch (std::exception const& e) { m_dispatch.reset(); if (!m_closed) ShowNotice(Wide(e.what()), InfoBarSeverity::Error); }
        catch (hresult_error const& e) { m_dispatch.reset(); if (!m_closed) ShowNotice(e.message(), InfoBarSeverity::Error); }
        m_dialogOpen = false;
        if (!m_closed) { UpdateActions(); if (needsRefresh) Refresh(); }
    }

    fire_and_forget MainWindow::ShowProperties()
    {
        auto lifetime = get_strong();
        auto selected = SelectedRows();
        if (selected.size() != 1 || ActionBusy() || m_dialogOpen) co_return;
        m_dialogOpen = true;
        UpdateActions();
        auto row = selected.front();
        auto controller = m_controller;
        auto object = Object(row);
        std::vector<std::pair<int, FrameworkElement>> editors;
        try
        {
            ContentDialog dialog;
            dialog.XamlRoot(Root().XamlRoot());
            dialog.RequestedTheme(Root().ActualTheme() == ElementTheme::Dark ? ElementTheme::Dark : ElementTheme::Light);
            dialog.Title(box_value(Wide(object->GetItemName())));
            if (std::ranges::any_of(controller->GetColumns(), [](auto const& c) { return c.Editable; }))
                dialog.PrimaryButtonText(L"Apply changes");
            dialog.CloseButtonText(L"Close");
            dialog.DefaultButton(ContentDialogButton::Close);
            StackPanel fields;
            fields.Spacing(12);
            fields.Margin({0, 0, 12, 0});
            for (int i = 0; i < static_cast<int>(controller->GetColumns().size()); ++i)
            {
                auto const& column = controller->GetColumns()[i];
                auto current = Wide(object->GetProperty(i));
                if (column.Editable && column.EditType == pserv::ColumnEditType::Combo)
                {
                    ComboBox combo;
                    combo.Header(box_value(Wide(column.DisplayName)));
                    combo.HorizontalAlignment(HorizontalAlignment::Stretch);
                    for (auto const& option : controller->GetComboOptions(i)) combo.Items().Append(box_value(Wide(option)));
                    for (uint32_t option = 0; option < combo.Items().Size(); ++option)
                        if (unbox_value<hstring>(combo.Items().GetAt(option)) == current) combo.SelectedIndex(option);
                    fields.Children().Append(combo);
                    editors.emplace_back(i, combo);
                }
                else
                {
                    TextBox text;
                    text.Header(box_value(Wide(column.DisplayName)));
                    text.Text(current);
                    text.IsReadOnly(!column.Editable);
                    text.AcceptsReturn(column.EditType == pserv::ColumnEditType::TextMultiline);
                    text.TextWrapping(TextWrapping::Wrap);
                    fields.Children().Append(text);
                    if (column.Editable) editors.emplace_back(i, text);
                }
            }
            ScrollViewer scroll;
            scroll.MaxHeight(540);
            scroll.HorizontalScrollMode(ScrollMode::Disabled);
            scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
            scroll.Content(fields);
            dialog.Content(scroll);
            ApplyFontTo(dialog);
            if (co_await dialog.ShowAsync() == ContentDialogResult::Primary && !m_closed)
            {
                std::vector<std::pair<int, std::string>> edits;
                for (auto const& [column, editor] : editors)
                {
                    hstring value;
                    if (auto text = editor.try_as<TextBox>()) value = text.Text();
                    else if (auto choice = editor.as<ComboBox>().SelectedItem()) value = unbox_value<hstring>(choice);
                    else continue;
                    if (Narrow(value) != object->GetProperty(column)) edits.emplace_back(column, Narrow(value));
                }
                if (!edits.empty())
                {
                    m_discardRefresh = m_refresh != nullptr;
                    m_dispatch = std::make_unique<pserv::DataActionDispatchContext>();
                    m_dispatch->m_pAsyncOp = new pserv::AsyncOperation();
                    // Re-read on the worker before editing; never mutate the displayed snapshot.
                    m_dispatch->m_pAsyncOp->Start(nullptr, [fresh = CreateController(m_activeProvider), id = object->GetStableID(), edits = std::move(edits)](pserv::AsyncOperation* op)
                    {
                        fresh->Refresh();
                        auto target = fresh->GetDataObjects().GetByStableId<pserv::DataObject>(id);
                        if (!target) throw std::runtime_error("The item no longer exists or could not be read.");
                        fresh->BeginPropertyEdits(target);
                        for (auto const& [column, value] : edits)
                            if (!fresh->SetPropertyEdit(target, column, value)) throw std::runtime_error("A property value was rejected.");
                        if (op->IsCancelRequested()) return false;
                        op->ReportProgress(0.5f, "Applying properties…");
                        if (!fresh->CommitPropertyEdits(target)) throw std::runtime_error("Could not apply properties. Check permissions and the diagnostic log.");
                        op->ReportProgress(1.0f, "Properties updated.");
                        return true;
                    });
                    ShowNotice(L"Applying properties…", InfoBarSeverity::Informational);
                    Progress().Visibility(Visibility::Visible);
                    CancelButton().Visibility(Visibility::Visible);
                }
            }
        }
        catch (hresult_error const& e) { if (!m_closed) ShowNotice(e.message(), InfoBarSeverity::Error); }
        catch (std::exception const& e) { if (!m_closed) ShowNotice(Wide(e.what()), InfoBarSeverity::Error); }
        m_dialogOpen = false;
        if (!m_closed) UpdateActions();
    }

    void MainWindow::AddVariableClicked(IInspectable const&, RoutedEventArgs const&) { AddVariable(); }

    fire_and_forget MainWindow::AddVariable()
    {
        auto lifetime = get_strong();
        if (m_activeProvider != 6 || ActionBusy() || m_dialogOpen) co_return;
        m_dialogOpen = true;
        UpdateActions();
        try
        {
            ContentDialog dialog;
            dialog.XamlRoot(Root().XamlRoot());
            dialog.RequestedTheme(Root().ActualTheme());
            dialog.Title(box_value(L"Add environment variable"));
            dialog.PrimaryButtonText(L"Create");
            dialog.CloseButtonText(L"Cancel");
            StackPanel fields;
            fields.Spacing(12);
            TextBox name;
            name.Header(box_value(L"Name"));
            TextBox value;
            value.Header(box_value(L"Value"));
            ComboBox scope;
            scope.Header(box_value(L"Scope"));
            scope.Items().Append(box_value(L"User"));
            scope.Items().Append(box_value(L"System (requires administrator access)"));
            scope.SelectedIndex(0);
            TextBlock error;
            error.TextWrapping(TextWrapping::Wrap);
            fields.Children().Append(name);
            fields.Children().Append(value);
            fields.Children().Append(scope);
            fields.Children().Append(error);
            dialog.Content(fields);
            dialog.PrimaryButtonClick([name, error](auto const&, ContentDialogButtonClickEventArgs const& e)
            {
                auto text = Narrow(name.Text());
                if (text.find_first_not_of(" \t\r\n") == std::string::npos || text.find('=') != std::string::npos)
                {
                    error.Text(L"Enter a nonempty name without an equals sign.");
                    e.Cancel(true);
                }
            });
            if (co_await dialog.ShowAsync() == ContentDialogResult::Primary && !m_closed)
            {
                auto variableName = Narrow(name.Text());
                auto variableValue = Narrow(value.Text());
                auto variableScope = scope.SelectedIndex() == 0 ? pserv::EnvironmentVariableScope::User : pserv::EnvironmentVariableScope::System;
                m_discardRefresh = m_refresh != nullptr;
                m_dispatch = std::make_unique<pserv::DataActionDispatchContext>();
                m_dispatch->m_pAsyncOp = new pserv::AsyncOperation();
                m_dispatch->m_pAsyncOp->Start(nullptr, [variableName, variableValue, variableScope](pserv::AsyncOperation* op)
                {
                    pserv::EnvironmentVariablesDataController fresh;
                    fresh.Refresh();
                    for (auto object : fresh.GetDataObjects())
                    {
                        auto existing = dynamic_cast<pserv::EnvironmentVariableInfo*>(object);
                        if (existing && existing->GetScope() == variableScope &&
                            _wcsicmp(pserv::utils::Utf8ToWide(existing->GetName()).c_str(), pserv::utils::Utf8ToWide(variableName).c_str()) == 0)
                            throw std::runtime_error("This variable already exists. Use Properties to edit it.");
                    }
                    if (op->IsCancelRequested()) return false;
                    op->ReportProgress(0.5f, "Creating environment variable…");
                    if (!pserv::EnvironmentVariableManager::SetEnvironmentVariable(variableName, variableValue, variableScope))
                        throw std::runtime_error("Could not create the variable. Check permissions and the diagnostic log.");
                    op->ReportProgress(1.0f, "Environment variable created.");
                    return true;
                });
                ShowNotice(L"Creating environment variable…", InfoBarSeverity::Informational);
                Progress().Visibility(Visibility::Visible);
                CancelButton().Visibility(Visibility::Visible);
            }
        }
        catch (hresult_error const& e) { if (!m_closed) ShowNotice(e.message(), InfoBarSeverity::Error); }
        catch (std::exception const& e) { if (!m_closed) ShowNotice(Wide(e.what()), InfoBarSeverity::Error); }
        m_dialogOpen = false;
        if (!m_closed) UpdateActions();
    }

    void MainWindow::ShowNotice(hstring const& message, InfoBarSeverity severity)
    {
        Notice().Severity(severity);
        Notice().Message(message);
        Notice().IsOpen(true);
    }
    void MainWindow::RefreshClicked(IInspectable const&, RoutedEventArgs const&) { Refresh(); }
    void MainWindow::SearchChanged(IInspectable const&, TextChangedEventArgs const&) { if (m_loaded && !m_switching) ApplyFilter(); }
    void MainWindow::SelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
    {
        if (!m_loaded || m_rebinding) return;
        auto started = std::chrono::steady_clock::now();
        UpdateActions();
        spdlog::debug("Selection update: {} us, {} selected",
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count(), Rows().SelectedItems().Size());
    }
    void MainWindow::ActionClicked(IInspectable const& sender, RoutedEventArgs const&) { ExecuteAction(Narrow(unbox_value<hstring>(sender.as<Button>().Tag()))); }
    void MainWindow::PropertiesClicked(IInspectable const&, RoutedEventArgs const&) { ShowProperties(); }
    void MainWindow::ActionsOpening(IInspectable const& sender, IInspectable const&) { FillActions(sender.as<MenuFlyout>()); }
    void MainWindow::AutoRefreshToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loaded && !m_switching) m_refreshPreference.UserToggled(AutoRefresh().IsOn());
    }
    void MainWindow::CancelClicked(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_dispatch && m_dispatch->m_pAsyncOp) m_dispatch->m_pAsyncOp->RequestCancel();
    }
    void MainWindow::RowsDoubleTapped(IInspectable const&, Input::DoubleTappedRoutedEventArgs const&) { ShowProperties(); }
    void MainWindow::RowsKeyDown(IInspectable const&, Input::KeyRoutedEventArgs const& e)
    {
        if (e.Key() == Windows::System::VirtualKey::Enter) { ShowProperties(); e.Handled(true); }
        else if ((GetKeyState(VK_CONTROL) & 0x8000) && e.Key() == Windows::System::VirtualKey::C)
        {
            ExecuteAction("Copy as Plain Text");
            e.Handled(true);
        }
    }
}
