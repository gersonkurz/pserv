#include "pch.h"
#include "MainWindow.xaml.h"
#include "providers.h"
#include <controllers/services_data_controller.h>
#include <windows_api/service_manager.h>
#include <utils/string_utils.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <shlobj.h>
#include <commdlg.h>
#include <dlgs.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;

namespace winrt::pserv6::implementation
{
    namespace
    {
        std::filesystem::path ExecutableDirectory()
        {
            std::wstring path(32768, L'\0');
            auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            check_bool(length && length < path.size());
            path.resize(length);
            return std::filesystem::path(path).parent_path();
        }
        std::filesystem::path PreferencesPath()
        {
            wil::unique_cotaskmem_string path;
            check_hresult(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, path.put()));
            return std::filesystem::path(path.get()) / L"pserv6" / L"appearance.toml";
        }
        Windows::UI::Color Color(pserv::theme::PaletteColor const& c)
        {
            return {255, static_cast<uint8_t>(c.x * 255), static_cast<uint8_t>(c.y * 255), static_cast<uint8_t>(c.z * 255)};
        }
    }

    void MainWindow::SetButtonContent(Button const& button, hstring const& label, wchar_t const* glyph)
    {
        FontIcon icon;
        icon.Glyph(glyph);
        icon.FontSize(16);
        button.Content(icon);
        button.Width(36);
        button.Height(32);
        Automation::AutomationProperties::SetName(button, label);
        ToolTipService::SetToolTip(button, box_value(label));
    }

    std::shared_ptr<pserv::DataController> MainWindow::CreateController(int index) const
    {
        auto controller = pserv::winui::Providers().at(index).create();
        if (auto services = dynamic_cast<pserv::ServicesDataController*>(controller.get())) services->SetMachineName(m_machineName);
        return controller;
    }

    void MainWindow::UpdateMachineLabel()
    {
        MachineLabel().Text(m_machineName.empty() ? L"LOCAL COMPUTER" : L"REMOTE COMPUTER");
        MachineText().Text(m_machineName.empty() ? hstring(m_localMachine) : to_hstring(m_machineName));
        AccessText().Text(m_machineName.empty() ? (m_elevated ? L"Local computer · Administrator" : L"Local computer · Standard access") :
            L"Remote services and drivers · Current Windows credentials");
    }

    void MainWindow::SetMachine(std::string name)
    {
        if (Busy() || m_dialogOpen) return;
        m_machineName = std::move(name);
        UpdateMachineLabel();
        for (uint32_t i = 0; i < Navigation().MenuItems().Size(); ++i)
            Navigation().MenuItems().GetAt(i).as<NavigationViewItem>().IsEnabled(m_machineName.empty() || i <= 1);
        m_activeProvider = -1;
        SelectProvider(0);
        Navigation().SelectedItem(Navigation().MenuItems().GetAt(0));
    }

    fire_and_forget MainWindow::ConnectRemote()
    {
        auto lifetime = get_strong();
        if (Busy() || m_dialogOpen) co_return;
        m_dialogOpen = true;
        UpdateActions();
        try
        {
            ContentDialog dialog;
            dialog.XamlRoot(Root().XamlRoot());
            dialog.RequestedTheme(Root().ActualTheme());
            dialog.Title(box_value(L"Connect to remote machine"));
            dialog.PrimaryButtonText(L"Connect");
            dialog.CloseButtonText(L"Cancel");
            StackPanel panel;
            panel.Spacing(12);
            TextBlock explanation;
            explanation.Text(L"Services and drivers support remote computers. Windows uses your current credentials; other views are available on the local computer only.");
            explanation.TextWrapping(TextWrapping::Wrap);
            TextBox machine;
            machine.Header(box_value(L"Computer name or address"));
            machine.Text(to_hstring(m_machineName));
            panel.Children().Append(explanation);
            panel.Children().Append(machine);
            dialog.Content(panel);
            ApplyFontTo(dialog);
            dialog.PrimaryButtonClick([machine](auto const&, ContentDialogButtonClickEventArgs const& e)
            {
                auto name = std::wstring(machine.Text());
                if (name.find_first_not_of(L" \t\r\n") == std::wstring::npos) e.Cancel(true);
            });
            if (co_await dialog.ShowAsync() == ContentDialogResult::Primary && !m_closed)
            {
                auto name = std::wstring(machine.Text());
                auto first = name.find_first_not_of(L" \t\r\n");
                auto last = name.find_last_not_of(L" \t\r\n");
                auto target = pserv::utils::WideToUtf8(name.substr(first, last - first + 1));
                m_pendingMachine = target;
                m_dispatch = std::make_unique<pserv::DataActionDispatchContext>();
                m_dispatch->m_pAsyncOp = new pserv::AsyncOperation;
                m_dispatch->m_pAsyncOp->Start(nullptr, [target](pserv::AsyncOperation* op)
                {
                    op->ReportProgress(0, "Connecting to " + target + "…");
                    pserv::ServiceManager manager(target);
                    if (!manager.IsConnected()) throw std::runtime_error("Cannot connect to the remote Service Control Manager. Check the name, network access and Windows permissions.");
                    op->ReportProgress(1, "Connected to " + target);
                    return !op->IsCancelRequested();
                });
                ShowNotice(L"Connecting…", InfoBarSeverity::Informational);
                Progress().Visibility(Visibility::Visible);
                CancelButton().Visibility(Visibility::Visible);
            }
        }
        catch (hresult_error const& e) { if (!m_closed) ShowNotice(e.message(), InfoBarSeverity::Error); }
        catch (std::exception const& e) { if (!m_closed) ShowNotice(to_hstring(e.what()), InfoBarSeverity::Error); }
        m_dialogOpen = false;
        if (!m_closed) UpdateActions();
    }

    void MainWindow::LoadAppearance()
    {

        m_themes.LoadFromDirectory((ExecutableDirectory() / L"themes").wstring());

        try
        {
            if (std::filesystem::exists(PreferencesPath()))
            {
                auto settings = toml::parse_file(PreferencesPath().string());
                m_themeName = settings["theme"].value_or(std::string("System"));
                m_fontFamily = pserv::utils::Utf8ToWide(settings["font"].value_or(std::string("Segoe UI Variable")));
                m_fontSize = std::clamp(settings["font_size"].value_or(13.0), 8.0, 32.0);
                m_fontWeight = static_cast<uint16_t>(std::clamp(settings["font_weight"].value_or(400), 100, 900));
                m_fontItalic = settings["font_italic"].value_or(false);
                m_refreshInterval = std::clamp(settings["refresh_ms"].value_or(5000), 1000, 60000);
                m_startProvider = std::clamp(settings["active_view"].value_or(0), 0, 9);
                if (auto views = settings["views"].as_array())
                    for (auto const& node : *views)
                    {
                        toml::node_view<const toml::node> entry{node};
                        int index = entry["id"].value_or(-1);
                        if (index < 0 || index > 9) continue;
                        ViewSettings view;
                        view.filter = to_hstring(entry["filter"].value_or(std::string{}));
                        view.sortColumn = entry["sort"].value_or(0);
                        view.ascending = entry["ascending"].value_or(true);
                        view.live = entry["live"].value_or(true);
                        if (auto widths = entry["widths"].as_array())
                            for (auto const& width : *widths) view.widths.push_back(std::clamp(width.value_or(180.0), 60.0, 900.0));
                        if (auto columns = entry["columns"].as_array())
                            for (auto const& column : *columns) view.columns.push_back(column.value_or(-1));
                        m_viewSettings[index] = std::move(view);
                    }
                int width = std::clamp(settings["window"]["width"].value_or(1360), 700, 10000);
                int height = std::clamp(settings["window"]["height"].value_or(860), 400, 10000);
                int x = settings["window"]["x"].value_or(100), y = settings["window"]["y"].value_or(100);
                RECT rectangle{x, y, x + width, y + height};
                if (MonitorFromRect(&rectangle, MONITOR_DEFAULTTONULL))
                    SetWindowPos(m_hWnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
                if (settings["window"]["maximized"].value_or(false)) ShowWindow(m_hWnd, SW_MAXIMIZE);
            }
        }
        catch (std::exception const& e) { spdlog::warn("Could not read appearance preferences: {}", e.what()); }
        BuildAppearanceMenu();
        ApplyFont();
        ApplyTheme();
    }

    void MainWindow::SaveAppearance()
    {
        try
        {
            SaveCurrentView();
            toml::table settings{{"theme", m_themeName}, {"font", pserv::utils::WideToUtf8(m_fontFamily)},
                {"font_size", m_fontSize}, {"font_weight", m_fontWeight}, {"font_italic", m_fontItalic}, {"refresh_ms", m_refreshInterval}, {"active_view", std::max(0, m_activeProvider)}};
            toml::array views;
            for (auto const& [id, view] : m_viewSettings)
            {
                toml::array widths, columns;
                for (auto width : view.widths) widths.push_back(width);
                for (auto column : view.columns) columns.push_back(column);
                views.push_back(toml::table{{"id", id}, {"widths", std::move(widths)}, {"columns", std::move(columns)},
                    {"sort", view.sortColumn}, {"ascending", view.ascending}, {"live", view.live}, {"filter", to_string(view.filter)}});
            }
            settings.insert("views", std::move(views));
            WINDOWPLACEMENT placement{sizeof(placement)};
            if (GetWindowPlacement(m_hWnd, &placement))
            {
                auto rc = placement.rcNormalPosition;
                settings.insert("window", toml::table{{"x", rc.left}, {"y", rc.top}, {"width", rc.right - rc.left},
                    {"height", rc.bottom - rc.top}, {"maximized", placement.showCmd == SW_SHOWMAXIMIZED}});
            }
            auto path = PreferencesPath();
            std::filesystem::create_directories(path.parent_path());
            auto temporary = path; temporary += L".tmp";
            std::ofstream file(temporary);
            file << settings;
            file.close();
            if (!file || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Writing preferences failed");
        }
        catch (std::exception const& e) { spdlog::warn("Could not save appearance preferences: {}", e.what()); }
    }

    void MainWindow::ApplyTheme()
    {
        auto resources = Application::Current().Resources();
        for (auto const& key : m_themeResources) resources.Remove(box_value(hstring(key)));
        m_themeResources.clear();
        m_customTheme = m_themes.SelectByName(m_themeName);
        auto theme = m_themeName == "Light" ? ElementTheme::Light : m_themeName == "Dark" ? ElementTheme::Dark :
            m_customTheme ? (m_themes.Current().isDark ? ElementTheme::Dark : ElementTheme::Light) : ElementTheme::Default;
        for (auto const& item : m_themeItems) item.IsChecked(to_string(unbox_value<hstring>(item.Tag())) == m_themeName);
        if (m_customTheme)
        {
            auto const& base = m_themes.Current().base;
            auto brush = [&](wchar_t const* key, int color)
            {
                resources.Insert(box_value(hstring(key)), Media::SolidColorBrush(Color(base[color])));
                m_themeResources.emplace_back(key);
            };
            for (auto key : {L"ApplicationPageBackgroundThemeBrush", L"LayerFillColorDefaultBrush", L"SolidBackgroundFillColorBaseBrush"}) brush(key, 0);
            for (auto key : {L"CardBackgroundFillColorDefaultBrush", L"ControlFillColorDefaultBrush", L"TextControlBackground", L"ComboBoxBackground", L"ContentDialogBackground", L"MenuFlyoutPresenterBackground", L"NavigationViewDefaultPaneBackground"}) brush(key, 1);
            for (auto key : {L"TextFillColorPrimaryBrush", L"TextControlForeground", L"ComboBoxForeground", L"MenuFlyoutItemForeground"}) brush(key, 5);
            brush(L"TextFillColorSecondaryBrush", 5);
            brush(L"TextFillColorDisabledBrush", 4);
            for (auto key : {L"ControlStrokeColorDefaultBrush", L"CardStrokeColorDefaultBrush", L"ControlFillColorSecondaryBrush", L"SubtleFillColorSecondaryBrush", L"ListViewItemBackgroundSelected"}) brush(key, 2);
            for (auto key : {L"AccentFillColorDefaultBrush", L"AccentFillColorSecondaryBrush", L"SystemControlHighlightAccentBrush"}) brush(key, 0x0D);
            resources.Insert(box_value(L"SystemAccentColor"), box_value(Color(base[0x0D])));
            m_themeResources.emplace_back(L"SystemAccentColor");
        }
        // Force ThemeResource references in existing controls to be re-evaluated.
        Root().RequestedTheme(theme == ElementTheme::Dark ? ElementTheme::Light : ElementTheme::Dark);
        Root().RequestedTheme(theme);
        Root().Background(m_customTheme ? Media::SolidColorBrush(Color(m_themes.Current().base[0])) :
            Media::SolidColorBrush(Root().ActualTheme() == ElementTheme::Dark ?
                Windows::UI::Color{255, 32, 32, 32} : Windows::UI::Color{255, 250, 250, 250}));
        Viewport().Background(Root().Background());
        UpdateRowColors();
    }

    namespace
    {
        UINT_PTR CALLBACK FontHook(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
        {
            if (message == WM_INITDIALOG)
                SetWindowLongPtrW(dialog, GWLP_USERDATA, reinterpret_cast<CHOOSEFONTW*>(lParam)->lCustData);
            if (message == WM_COMMAND && LOWORD(wParam) == psh3)
            {
                auto apply = reinterpret_cast<std::function<void(LOGFONTW const&)>*>(GetWindowLongPtrW(dialog, GWLP_USERDATA));
                LOGFONTW font{};
                SendMessageW(dialog, WM_CHOOSEFONT_GETLOGFONT, 0, reinterpret_cast<LPARAM>(&font));
                if (apply) (*apply)(font);
            }
            return 0;
        }
    }

    void MainWindow::ChooseUIFont()
    {
        if (m_dialogOpen || ActionBusy()) return;
        auto originalFont = m_fontFamily;
        auto originalSize = m_fontSize;
        auto originalWeight = m_fontWeight;
        auto originalItalic = m_fontItalic;
        m_dialogOpen = true;
        UpdateActions();
        LOGFONTW font{};
        wcsncpy_s(font.lfFaceName, m_fontFamily.c_str(), _TRUNCATE);
        auto dpi = GetDpiForWindow(m_hWnd);
        font.lfHeight = -MulDiv(static_cast<int>(m_fontSize), dpi, 96);
        font.lfWeight = m_fontWeight;
        font.lfItalic = m_fontItalic;
        std::function<void(LOGFONTW const&)> apply = [&](LOGFONTW const& selected)
        {
            m_fontFamily = selected.lfFaceName;
            m_fontWeight = static_cast<uint16_t>(selected.lfWeight);
            m_fontItalic = selected.lfItalic != 0;
            m_fontSize = std::clamp(std::abs(selected.lfHeight) * 96.0 / dpi, 8.0, 32.0);
            ApplyFont();
        };
        CHOOSEFONTW chooser{sizeof(chooser)};
        chooser.hwndOwner = m_hWnd;
        chooser.lpLogFont = &font;
        chooser.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_ENABLEHOOK | CF_APPLY | CF_NOVERTFONTS | CF_FORCEFONTEXIST;
        chooser.lpfnHook = FontHook;
        chooser.lCustData = reinterpret_cast<LPARAM>(&apply);
        chooser.nSizeMin = 6;
        chooser.nSizeMax = 24;
        chooser.Flags |= CF_LIMITSIZE;
        if (::ChooseFontW(&chooser)) { apply(font); SaveAppearance(); }
        else
        {
            m_fontFamily = originalFont;
            m_fontSize = originalSize;
            m_fontWeight = originalWeight;
            m_fontItalic = originalItalic;
            ApplyFont();
            if (auto error = CommDlgExtendedError()) ShowNotice(to_hstring(std::format("Font dialog failed: {}", error)), InfoBarSeverity::Error);
        }
        m_dialogOpen = false;
        UpdateActions();
    }

    void MainWindow::ApplyFontTo(DependencyObject const& node)
    {
        if (!node || node.try_as<FontIcon>() || node.try_as<SymbolIcon>()) return;
        if (auto control = node.try_as<Control>())
        {
            control.FontFamily(Media::FontFamily(m_fontFamily));
            control.FontWeight({m_fontWeight});
            control.FontStyle(m_fontItalic ? Windows::UI::Text::FontStyle::Italic : Windows::UI::Text::FontStyle::Normal);
        }
        if (auto text = node.try_as<TextBlock>())
        {
            text.FontFamily(Media::FontFamily(m_fontFamily));
            text.FontWeight({m_fontWeight});
            text.FontStyle(m_fontItalic ? Windows::UI::Text::FontStyle::Italic : Windows::UI::Text::FontStyle::Normal);
        }
        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(node); ++i) ApplyFontTo(Media::VisualTreeHelper::GetChild(node, i));
    }

    void MainWindow::ApplyFont()
    {
        Application::Current().Resources().Insert(box_value(L"ContentControlThemeFontFamily"), Media::FontFamily(m_fontFamily));
        ApplyFontTo(Root());
        for (auto const& row : m_allRows)
            for (auto const& cell : row.Cells()) { auto impl = get_self<implementation::DataCell>(cell); impl->SetFontName(hstring(m_fontFamily)); impl->SetFontStyle(m_fontWeight, m_fontItalic); }
        ApplyZoom();
    }

    void MainWindow::SaveCurrentView()
    {
        if (m_activeProvider >= 0)
            m_viewSettings[m_activeProvider] = {m_widths, SearchBox().Text(), m_sortColumn, m_ascending, AutoRefresh().IsOn(), m_columns};
    }

    void MainWindow::UpdateCounts()
    {
        m_disabledCount = m_highlightedCount = 0;
        for (auto const& row : m_allRows)
        {
            auto state = m_controller->GetVisualState(get_self<implementation::DataRow>(row)->Object());
            m_disabledCount += state == pserv::VisualState::Disabled;
            m_highlightedCount += state == pserv::VisualState::Highlighted;
        }
    }

    void MainWindow::BuildAppearanceMenu()
    {
        auto makeTheme = [&](std::string const& name)
        {
            ToggleMenuFlyoutItem item;
            item.Text(to_hstring(name));
            item.Tag(box_value(to_hstring(name)));
            item.FontFamily(Media::FontFamily(m_fontFamily));
            item.Click([weak = get_weak(), name](auto const&, auto const&)
            {
                if (auto self = weak.get()) { self->m_themeName = name; self->ApplyTheme(); self->SaveAppearance(); }
            });
            m_themeItems.push_back(item);
            return item;
        };
        ThemesMenu().Items().InsertAt(0, makeTheme("System"));
        ThemesMenu().Items().InsertAt(1, MenuFlyoutSeparator());
        DarkThemes().Items().Append(makeTheme("Dark"));
        LightThemes().Items().Append(makeTheme("Light"));
        for (auto const& theme : m_themes.Themes())
            (theme.dark ? DarkThemes() : LightThemes()).Items().Append(makeTheme(theme.name));
        for (int interval : {1000, 2000, 5000, 10000, 30000, 60000})
        {
            ToggleMenuFlyoutItem item;
            item.Text(to_hstring(interval / 1000) + L" seconds");
            item.Tag(box_value(interval));
            item.IsChecked(interval == m_refreshInterval);
            item.Click([weak = get_weak(), interval](auto const&, auto const&)
            {
                if (auto self = weak.get())
                {
                    self->m_refreshInterval = interval;
                    self->AutoRefresh().OnContent(box_value(to_hstring(interval / 1000) + L"s"));
                    for (auto const& entry : self->m_intervalItems) entry.IsChecked(unbox_value<int>(entry.Tag()) == interval);
                    self->SaveAppearance();
                }
            });
            m_intervalItems.push_back(item);
            RefreshIntervals().Items().Append(item);
        }
        AutoRefresh().OnContent(box_value(to_hstring(m_refreshInterval / 1000) + L"s"));
    }

    fire_and_forget MainWindow::ChooseColumns()
    {
        auto lifetime = get_strong();
        if (m_dialogOpen || ActionBusy() || !m_controller) co_return;
        m_dialogOpen = true;
        UpdateActions();
        try
        {
            struct ColumnChoice { int id; bool visible; };
            auto choices = std::make_shared<std::vector<ColumnChoice>>();
            for (auto column : m_columns) choices->push_back({column, true});
            auto metadata = m_controller->GetColumns();
            for (int column = 0; column < static_cast<int>(metadata.size()); ++column)
                if (std::ranges::find(m_columns, column) == m_columns.end()) choices->push_back({column, false});
            StackPanel list;
            list.Spacing(4);
            std::function<void()> render;
            render = [&]
            {
                list.Children().Clear();
                for (size_t i = 0; i < choices->size(); ++i)
                {
                    Grid row;
                    row.ColumnDefinitions().Append(ColumnDefinition{});
                    ColumnDefinition buttonsColumn; buttonsColumn.Width(GridLengthHelper::Auto());
                    row.ColumnDefinitions().Append(buttonsColumn);
                    CheckBox checkbox;
                    checkbox.Content(box_value(to_hstring(metadata[(*choices)[i].id].DisplayName)));
                    checkbox.IsChecked((*choices)[i].visible);
                    checkbox.Checked([choices, i](auto const&, auto const&) { (*choices)[i].visible = true; });
                    checkbox.Unchecked([choices, i](auto const&, auto const&) { (*choices)[i].visible = false; });
                    row.Children().Append(checkbox);
                    StackPanel buttons; buttons.Orientation(Orientation::Horizontal);
                    for (int direction : {-1, 1})
                    {
                        Button move;
                        SetButtonContent(move, direction < 0 ? L"Move up" : L"Move down", direction < 0 ? L"\uE70E" : L"\uE70D");
                        move.IsEnabled(direction < 0 ? i > 0 : i + 1 < choices->size());
                        move.Click([&, choices, i, direction](auto const&, auto const&)
                        {
                            std::swap((*choices)[i], (*choices)[static_cast<size_t>(static_cast<int>(i) + direction)]);
                            render();
                        });
                        buttons.Children().Append(move);
                    }
                    Grid::SetColumn(buttons, 1); row.Children().Append(buttons);
                    list.Children().Append(row);
                }
                ApplyFontTo(list);
            };
            render();
            ScrollViewer scroll; scroll.MaxHeight(500); scroll.Content(list);
            ContentDialog dialog;
            dialog.XamlRoot(Root().XamlRoot()); dialog.RequestedTheme(Root().ActualTheme());
            dialog.Title(box_value(L"Columns — show, hide and reorder"));
            dialog.Content(scroll); dialog.PrimaryButtonText(L"Apply"); dialog.CloseButtonText(L"Cancel");
            dialog.PrimaryButtonClick([choices](auto const&, ContentDialogButtonClickEventArgs const& e)
            { if (std::ranges::none_of(*choices, [](auto c) { return c.visible; })) e.Cancel(true); });
            ApplyFontTo(dialog);
            if (co_await dialog.ShowAsync() == ContentDialogResult::Primary && !m_closed)
            {
                std::unordered_map<int, double> widths;
                for (size_t i = 0; i < m_columns.size(); ++i) widths[m_columns[i]] = m_widths[i];
                m_columns.clear(); m_widths.clear();
                for (auto const& choice : *choices) if (choice.visible)
                {
                    m_columns.push_back(choice.id);
                    m_widths.push_back(widths.contains(choice.id) ? widths[choice.id] : 180.0);
                }
                m_sortColumn = 0;
                SaveCurrentView();
                auto provider = m_activeProvider;
                m_activeProvider = -1;
                m_dialogOpen = false;
                SelectProvider(provider);
                SaveAppearance();
            }
        }
        catch (hresult_error const& error) { if (!m_closed) ShowNotice(error.message(), InfoBarSeverity::Error); }
        m_dialogOpen = false;
        if (!m_closed) UpdateActions();
    }

    fire_and_forget MainWindow::ShowAbout()
    {
        auto lifetime = get_strong();
        if (m_dialogOpen || ActionBusy()) co_return;
        m_dialogOpen = true;
        UpdateActions();
        try
        {
            ContentDialog dialog;
            dialog.XamlRoot(Root().XamlRoot());
            dialog.RequestedTheme(Root().ActualTheme());
            dialog.Title(box_value(L"About pserv6"));
            dialog.Content(box_value(L"pserv6 — Windows system administration\n\nNative C++/WinRT · WinUI 3 · Windows App SDK 2.7.0\n\nCopyright © 2025–2026 Gerson Kurz\nMIT License\n\nIn development. See the documentation for current feature coverage and validation."));
            dialog.PrimaryButtonText(L"Documentation");
            dialog.CloseButtonText(L"Close");
            if (co_await dialog.ShowAsync() == ContentDialogResult::Primary && !m_closed) OpenDocumentation();
        }
        catch (hresult_error const& e) { if (!m_closed) ShowNotice(e.message(), InfoBarSeverity::Error); }
        m_dialogOpen = false;
        if (!m_closed) UpdateActions();
    }

    void MainWindow::OpenDocumentation()
    {
        auto path = ExecutableDirectory() / L"docs" / L"pserv6.md";
        auto result = ShellExecuteW(m_hWnd, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32) ShowNotice(L"Could not open the manual. It is in docs/pserv6.md beside pserv6.exe.", InfoBarSeverity::Warning);
    }

    void MainWindow::Elevate()
    {
        if (Busy() || m_dialogOpen || m_elevated) return;
        auto path = ExecutableDirectory() / L"pserv6.exe";
        SHELLEXECUTEINFOW info{sizeof(info)};
        info.hwnd = m_hWnd;
        info.lpVerb = L"runas";
        info.lpFile = path.c_str();
        info.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&info))
        {
            auto error = GetLastError();
            if (error != ERROR_CANCELLED) ShowNotice(L"Could not launch an elevated instance.", InfoBarSeverity::Error);
            return;
        }
        // The new process opens locally; keep this window available so its view
        // and any remote connection are not silently lost.
        ShowNotice(L"An administrator window has been opened on the local computer.", InfoBarSeverity::Informational);
    }

    void MainWindow::CommandClicked(IInspectable const& sender, RoutedEventArgs const&)
    {
        auto command = to_string(unbox_value<hstring>(sender.as<FrameworkElement>().Tag()));
        if (command == "local") SetMachine("");
        else if (command == "remote") ConnectRemote();
        else if (command == "font") ChooseUIFont();
        else if (command == "columns") ChooseColumns();
        else if (command == "about") ShowAbout();
        else if (command == "documentation") OpenDocumentation();
        else if (command == "elevate") Elevate();
        else if (command == "exit") Close();
        else if (command == "reset-zoom") { m_fontSize = 13; ApplyZoom(); }
        else if (command == "select-all") Rows().SelectAll();
        else ExecuteAction(command);
    }
}
