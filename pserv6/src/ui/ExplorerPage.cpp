#include "pch.h"
#include "ExplorerPage.h"

#include <format>
#include <ranges>
#include <type_traits>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace
{
    Brush ThemeBrush(winrt::hstring const &key)
    {
        return Application::Current().Resources().Lookup(winrt::box_value(key)).as<Brush>();
    }

    std::string ValueToString(const pserv6::interaction::Value &value)
    {
        return std::visit(
            [](const auto &v) -> std::string
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                {
                    return {};
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    return v ? "true" : "false";
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    return v;
                }
                else
                {
                    return std::to_string(v);
                }
            },
            value);
    }

    TextBlock MakeText(std::wstring const &text, double fontSize, winrt::hstring const &resourceKey)
    {
        TextBlock tb;
        tb.Text(text);
        tb.FontSize(fontSize);
        tb.TextWrapping(TextWrapping::Wrap);
        tb.Foreground(ThemeBrush(resourceKey));
        return tb;
    }

    std::wstring ToWide(const std::string &text)
    {
        if (text.empty())
        {
            return {};
        }

        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        std::wstring result(static_cast<size_t>(size > 0 ? size - 1 : 0), L'\0');
        if (size > 1)
        {
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
        }
        return result;
    }
}

ExplorerPage::ExplorerPage()
    : m_adapter{std::make_unique<pserv6::interaction::ServicesViewAdapter>()}
    , m_actionExecutor{std::make_unique<pserv6::interaction::ServicesActionExecutor>(*m_adapter)}
{
    BuildShell();
    Refresh();
}

winrt::Microsoft::UI::Xaml::UIElement ExplorerPage::Root() const
{
    return m_root;
}

bool ExplorerPage::Confirm(const pserv6::interaction::ConfirmationRequest &request)
{
    const auto title = ToWide(request.Title);
    const auto message = ToWide(request.Message);
    return MessageBoxW(nullptr, message.c_str(), title.c_str(), MB_YESNO | MB_ICONWARNING) == IDYES;
}

void ExplorerPage::ReportProgress(const pserv6::interaction::ActionProgress &progress)
{
    m_statusText.Text(winrt::hstring{ToWide(std::format("{} ({:.0f}%)", progress.Message, progress.Progress * 100.0f))});
}

bool ExplorerPage::IsCancellationRequested() const
{
    return false;
}

void ExplorerPage::ReportError(std::string_view message)
{
    m_statusText.Text(winrt::hstring{ToWide(std::string(message))});
}

void ExplorerPage::BuildShell()
{
    Grid root;
    root.Padding(ThicknessHelper::FromLengths(24, 24, 24, 24));
    root.ColumnSpacing(20);

    auto leftColumn = ColumnDefinition{};
    leftColumn.Width(GridLengthHelper::FromValueAndType(2, GridUnitType::Star));
    auto rightColumn = ColumnDefinition{};
    rightColumn.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    root.ColumnDefinitions().Append(leftColumn);
    root.ColumnDefinitions().Append(rightColumn);

    m_layout = StackPanel{};
    m_layout.Spacing(12);

    auto title = MakeText(L"Explorer Shell", 28.0, L"TextFillColorPrimaryBrush");
    title.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    auto subtitle = MakeText(L"Generic explorer surface driven by the interaction contracts.", 14.0, L"TextFillColorSecondaryBrush");

    m_actionsPanel = StackPanel{};
    m_actionsPanel.Orientation(Orientation::Horizontal);
    m_actionsPanel.Spacing(8);

    m_statusText = MakeText(L"Ready.", 13.0, L"TextFillColorSecondaryBrush");

    auto listHost = Border{};
    listHost.Padding(ThicknessHelper::FromLengths(12, 12, 12, 12));
    listHost.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
    listHost.Background(ThemeBrush(L"SubtleFillColorSecondaryBrush"));

    m_listPanel = StackPanel{};
    m_listPanel.Spacing(6);
    listHost.Child(m_listPanel);

    m_layout.Children().Append(title);
    m_layout.Children().Append(subtitle);
    m_layout.Children().Append(m_actionsPanel);
    m_layout.Children().Append(m_statusText);
    m_layout.Children().Append(listHost);

    Grid::SetColumn(m_layout, 0);
    root.Children().Append(m_layout);

    auto detailsHost = Border{};
    detailsHost.Padding(ThicknessHelper::FromLengths(12, 12, 12, 12));
    detailsHost.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
    detailsHost.Background(ThemeBrush(L"SubtleFillColorSecondaryBrush"));

    m_detailsPanel = StackPanel{};
    m_detailsPanel.Spacing(8);
    detailsHost.Child(m_detailsPanel);

    Grid::SetColumn(detailsHost, 1);
    root.Children().Append(detailsHost);

    m_root = root;
}

void ExplorerPage::Refresh()
{
    const auto result = m_adapter->Refresh(m_state, false);
    m_items = result.Items;

    if (result.ErrorMessage.has_value())
    {
        ReportError(*result.ErrorMessage);
    }
    else
    {
        m_statusText.Text(L"Loaded view.");
    }

    if (m_state.SelectedItemIds.empty() && !m_items.empty())
    {
        m_state.SelectedItemIds = {m_items.front().Id};
        m_state.FocusedItemId = m_items.front().Id;
    }

    RenderActions();
    RenderList();
    RenderDetails();
}

void ExplorerPage::RenderActions()
{
    m_actionsPanel.Children().Clear();

    auto refreshButton = Button{};
    refreshButton.Content(winrt::box_value(L"Refresh"));
    refreshButton.Click([this](auto &&, auto &&) { Refresh(); });
    m_actionsPanel.Children().Append(refreshButton);

    for (const auto &action : m_adapter->GetActions(m_state))
    {
        auto button = Button{};
        button.Content(winrt::box_value(winrt::hstring{ToWide(action.Label)}));
        button.IsEnabled(action.Enabled);
        button.Click([this, actionId = action.Id](auto &&, auto &&) { ExecuteAction(actionId); });
        m_actionsPanel.Children().Append(button);
    }
}

void ExplorerPage::RenderList()
{
    m_listPanel.Children().Clear();

    const auto &descriptor = m_adapter->Describe();

    auto header = MakeText(ToWide(descriptor.Title), 20.0, L"TextFillColorPrimaryBrush");
    header.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    m_listPanel.Children().Append(header);

    for (const auto &item : m_items)
    {
        auto rowBorder = Border{};
        rowBorder.Padding(ThicknessHelper::FromLengths(10, 8, 10, 8));
        rowBorder.CornerRadius(CornerRadiusHelper::FromUniformRadius(6));
        rowBorder.Margin(ThicknessHelper::FromLengths(0, 0, 0, 4));

        const bool selected = std::ranges::find(m_state.SelectedItemIds, item.Id) != m_state.SelectedItemIds.end();
        if (selected)
        {
            rowBorder.Background(ThemeBrush(L"AccentFillColorSecondaryBrush"));
        }
        else
        {
            rowBorder.Background(ThemeBrush(L"ControlFillColorDefaultBrush"));
        }

        auto row = StackPanel{};
        row.Spacing(4);

        std::wstring line;
        for (size_t i = 0; i < std::min(item.Values.size(), descriptor.Columns.size()); ++i)
        {
            if (i > 0)
            {
                line += L"  |  ";
            }
            line += ToWide(ValueToString(item.Values[i]));
        }

        auto text = MakeText(line, 13.0, L"TextFillColorPrimaryBrush");
        row.Children().Append(text);

        auto stateText = MakeText(
            item.State == pserv6::interaction::VisualState::Highlighted ? L"Highlighted" :
            item.State == pserv6::interaction::VisualState::Disabled ? L"Disabled" : L"Normal",
            12.0,
            L"TextFillColorSecondaryBrush");
        row.Children().Append(stateText);

        rowBorder.Child(row);
        rowBorder.Tapped([this, itemId = item.Id](auto &&, auto &&) { SelectItem(itemId); });

        m_listPanel.Children().Append(rowBorder);
    }
}

void ExplorerPage::RenderDetails()
{
    m_detailsPanel.Children().Clear();

    auto title = MakeText(L"Details", 20.0, L"TextFillColorPrimaryBrush");
    title.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    m_detailsPanel.Children().Append(title);

    if (!m_state.FocusedItemId.has_value())
    {
        m_detailsPanel.Children().Append(MakeText(L"No selection.", 13.0, L"TextFillColorSecondaryBrush"));
        return;
    }

    const auto details = m_adapter->GetDetails(*m_state.FocusedItemId);
    if (!details.has_value())
    {
        m_detailsPanel.Children().Append(MakeText(L"No details available.", 13.0, L"TextFillColorSecondaryBrush"));
        return;
    }

    m_detailsPanel.Children().Append(MakeText(ToWide(details->Title), 16.0, L"TextFillColorPrimaryBrush"));

    for (const auto &section : details->Sections)
    {
        auto sectionBorder = Border{};
        sectionBorder.Padding(ThicknessHelper::FromLengths(10, 10, 10, 10));
        sectionBorder.CornerRadius(CornerRadiusHelper::FromUniformRadius(6));
        sectionBorder.Background(ThemeBrush(L"ControlFillColorDefaultBrush"));
        sectionBorder.Margin(ThicknessHelper::FromLengths(0, 0, 0, 8));

        auto sectionPanel = StackPanel{};
        sectionPanel.Spacing(4);

        auto sectionTitle = MakeText(ToWide(section.Title), 14.0, L"TextFillColorPrimaryBrush");
        sectionTitle.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        sectionPanel.Children().Append(sectionTitle);

        for (const auto &field : section.Fields)
        {
            const auto line = std::format("{}: {}", field.Label, ValueToString(field.Value));
            auto fieldText = MakeText(ToWide(line), 12.5, field.Editable ? L"TextFillColorPrimaryBrush" : L"TextFillColorSecondaryBrush");
            sectionPanel.Children().Append(fieldText);
        }

        sectionBorder.Child(sectionPanel);
        m_detailsPanel.Children().Append(sectionBorder);
    }
}

void ExplorerPage::SelectItem(const std::string &itemId)
{
    m_state.SelectedItemIds = {itemId};
    m_state.FocusedItemId = itemId;
    RenderActions();
    RenderList();
    RenderDetails();
}

void ExplorerPage::ExecuteAction(const std::string &actionId)
{
    pserv6::interaction::ActionRequest request;
    request.ViewId = m_adapter->Describe().Id;
    request.ActionId = actionId;
    request.ItemIds = m_state.SelectedItemIds;

    const auto result = m_actionExecutor->Execute(request, *this);
    if (result.ErrorMessage.has_value())
    {
        ReportError(*result.ErrorMessage);
    }
    else if (result.Cancelled)
    {
        m_statusText.Text(L"Action cancelled.");
    }
    else if (result.Success)
    {
        m_statusText.Text(L"Action completed.");
    }

    if (result.RequiresRefresh)
    {
        Refresh();
    }
}
