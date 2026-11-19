#pragma once
#include "MainWindow.g.h"
#include "DataRow.h"
#include <core/data_controller.h>
#include <core/data_action_dispatch_context.h>
#include <core/async_operation.h>
#include <theme/base16_theme.h>

namespace winrt::pserv6::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow() = default;
        ~MainWindow();
        void CommandClicked(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void RowsLoaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void RowContentChanging(Microsoft::UI::Xaml::Controls::ListViewBase const&, Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const&);
        void ProviderChanged(Microsoft::UI::Xaml::Controls::NavigationView const&, Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const&);
        void AddVariableClicked(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void OnLoaded(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void RefreshClicked(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void SearchChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::TextChangedEventArgs const&);
        void SelectionChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const&);
        void ActionClicked(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void PropertiesClicked(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void ActionsOpening(Windows::Foundation::IInspectable const&, Windows::Foundation::IInspectable const&);
        void CancelClicked(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
        void RowsDoubleTapped(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const&);
        void RowsRightTapped(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const&);
        void RowsKeyDown(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const&);
        void ViewportSizeChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::SizeChangedEventArgs const&);
    private:
        std::shared_ptr<pserv::DataController> m_controller;
        std::shared_ptr<pserv::DataController> m_pendingController;
        std::unique_ptr<pserv::AsyncOperation> m_refresh;
        std::unique_ptr<pserv::DataActionDispatchContext> m_dispatch;
        Microsoft::UI::Dispatching::DispatcherQueueTimer m_timer{nullptr};
        std::vector<pserv6::DataRow> m_allRows;
        std::vector<pserv6::DataRow> m_visibleRows;
        Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> m_items{nullptr};
        Microsoft::UI::Xaml::Media::Brush m_normalBrush{nullptr};
        Microsoft::UI::Xaml::Media::Brush m_disabledBrush{nullptr};
        Microsoft::UI::Xaml::Media::Brush m_runningBrush{nullptr};
        double m_fontSize{13};
        uint16_t m_fontWeight{400};
        bool m_fontItalic{false};
        Microsoft::UI::Xaml::Controls::ScrollViewer m_rowScroll{nullptr};
        pserv::theme::ThemeSet m_themes;
        bool m_customTheme{false};
        std::string m_themeName{"System"};
        int m_refreshInterval{5000};
        int m_startProvider{0};
        size_t m_disabledCount{}, m_highlightedCount{};
        std::vector<Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem> m_themeItems;
        std::vector<Microsoft::UI::Xaml::Controls::ToggleMenuFlyoutItem> m_intervalItems;
        void BuildAppearanceMenu();
        void ApplyFont();
        void ApplyFontTo(Microsoft::UI::Xaml::DependencyObject const& node);
        void SaveCurrentView();
        void UpdateCounts();
        winrt::fire_and_forget ChooseColumns();
        void ChooseUIFont();
        bool m_elevated{false};
        std::string m_machineName;
        std::optional<std::string> m_pendingMachine;
        std::wstring m_localMachine;
        std::wstring m_fontFamily{L"Segoe UI Variable"};
        std::vector<std::wstring> m_themeResources;
        bool m_discardRefresh{false};
        std::vector<double> m_widths;
        std::vector<int> m_columns;
        int m_activeProvider{-1};
        int m_refreshProvider{-1};
        bool m_switching{false};
        struct ViewSettings
        {
            std::vector<double> widths;
            hstring filter;
            int sortColumn{0};
            bool ascending{true};
            bool live{true};
            std::vector<int> columns;
        };
        std::unordered_map<int, ViewSettings> m_viewSettings;
        int m_sortColumn{0};
        bool m_ascending{true};
        bool m_loaded{false};
        bool m_rebinding{false};
        bool m_dialogOpen{false};
        bool m_closed{false};
        HWND m_hWnd{nullptr};
        std::chrono::steady_clock::time_point m_lastRefresh{};
        std::chrono::steady_clock::time_point m_refreshStarted{};
        bool Busy() const;
        bool ActionBusy() const;
        void UpdateRowColors();
        void ApplyZoom();
        void WheelChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const&);
        void BuildNavigation();
        void LoadAppearance();
        void ApplyTheme();
        void SaveAppearance();
        void UpdateMachineLabel();
        void SetMachine(std::string name);
        std::shared_ptr<pserv::DataController> CreateController(int index) const;
        void SetButtonContent(Microsoft::UI::Xaml::Controls::Button const& button, hstring const& label, wchar_t const* glyph);
        winrt::fire_and_forget ConnectRemote();
        winrt::fire_and_forget ShowAbout();
        void OpenDocumentation();
        void Elevate();
        void SelectProvider(int index);
        winrt::fire_and_forget AddVariable();
        void Refresh();
        void Tick();
        void RebuildRows();
        void RenderRow(Microsoft::UI::Xaml::Controls::Grid const& grid, pserv6::DataRow const& row);
        void RefreshRealizedRows();
        void ApplyFilter();
        void UpdateActions();
        void BuildHeaders();
        void ApplyWidths(Microsoft::UI::Xaml::Controls::Grid const& grid);
        std::vector<pserv6::DataRow> SelectedRows();
        std::vector<const pserv::DataAction*> AvailableActions();
        void FillActions(Microsoft::UI::Xaml::Controls::MenuFlyout const& menu);
        winrt::fire_and_forget ExecuteAction(std::string name);
        winrt::fire_and_forget ShowProperties();
        void ShowNotice(hstring const& message, Microsoft::UI::Xaml::Controls::InfoBarSeverity severity);
    };
}
namespace winrt::pserv6::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}
