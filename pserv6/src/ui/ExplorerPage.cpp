#include "pch.h"
#include "ExplorerPage.h"

#include <format>
#include <ranges>
#include <unordered_map>
#include <type_traits>

using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;
using namespace winrt::Microsoft::UI::Xaml::Input;

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

    std::string ToUtf8(std::wstring_view text)
    {
        if (text.empty())
        {
            return {};
        }

        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<size_t>(size > 0 ? size : 0), '\0');
        if (size > 0)
        {
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
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

winrt::Windows::Foundation::IAsyncAction ExplorerPage::ShowPropertiesDialogAsync()
{
    if (!m_state.FocusedItemId.has_value())
    {
        co_return;
    }

    auto details = m_adapter->GetDetails(*m_state.FocusedItemId);
    if (!details.has_value())
    {
        co_return;
    }

    ContentDialog dialog;
    dialog.Title(winrt::box_value(winrt::hstring{ToWide(std::format("Properties - {}", details->Title))}));
    dialog.PrimaryButtonText(L"Apply");
    dialog.CloseButtonText(L"Cancel");
    dialog.DefaultButton(ContentDialogButton::Primary);
    dialog.XamlRoot(m_root.XamlRoot());

    ScrollViewer scroll;
    scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    scroll.MaxHeight(700);

    StackPanel content;
    content.Spacing(12);

    std::unordered_map<std::string, TextBox> textFields;
    std::unordered_map<std::string, ComboBox> comboFields;

    for (const auto &section : details->Sections)
    {
        Border sectionBorder;
        sectionBorder.Padding(ThicknessHelper::FromLengths(12, 12, 12, 12));
        sectionBorder.CornerRadius(CornerRadiusHelper::FromUniformRadius(8));
        sectionBorder.Background(ThemeBrush(L"SubtleFillColorSecondaryBrush"));

        StackPanel sectionPanel;
        sectionPanel.Spacing(8);

        auto sectionTitle = MakeText(ToWide(section.Title), 15.0, L"TextFillColorPrimaryBrush");
        sectionTitle.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        sectionPanel.Children().Append(sectionTitle);

        for (const auto &field : section.Fields)
        {
            StackPanel fieldPanel;
            fieldPanel.Spacing(4);

            auto label = MakeText(ToWide(field.Label), 12.0, L"TextFillColorSecondaryBrush");
            fieldPanel.Children().Append(label);

            const auto currentValue = winrt::hstring{ToWide(ValueToString(field.Value))};

            if (field.Editable && field.Editor == pserv6::interaction::FieldEditor::Combo)
            {
                ComboBox combo;
                combo.MinWidth(260);
                for (const auto &option : field.Options)
                {
                    ComboBoxItem item;
                    item.Content(winrt::box_value(winrt::hstring{ToWide(option.Label)}));
                    item.Tag(winrt::box_value(winrt::hstring{ToWide(option.Id)}));
                    combo.Items().Append(item);

                    if (option.Label == ValueToString(field.Value))
                    {
                        combo.SelectedItem(item);
                    }
                }
                fieldPanel.Children().Append(combo);
                comboFields[field.Id] = combo;
            }
            else
            {
                TextBox box;
                box.Text(currentValue);
                box.IsReadOnly(!field.Editable);
                if (field.Editor == pserv6::interaction::FieldEditor::MultilineText)
                {
                    box.AcceptsReturn(true);
                    box.TextWrapping(TextWrapping::Wrap);
                    box.MinHeight(96);
                }
                fieldPanel.Children().Append(box);
                textFields[field.Id] = box;
            }

            sectionPanel.Children().Append(fieldPanel);
        }

        sectionBorder.Child(sectionPanel);
        content.Children().Append(sectionBorder);
    }

    scroll.Content(content);
    dialog.Content(scroll);

    dialog.PrimaryButtonClick([this, &dialog, details, textFields, comboFields](
                                  ContentDialog const &,
                                  ContentDialogButtonClickEventArgs const &args) mutable
    {
        auto deferral = args.GetDeferral();

        for (auto &section : details->Sections)
        {
            for (auto &field : section.Fields)
            {
                if (!field.Editable)
                {
                    continue;
                }

                if (field.Editor == pserv6::interaction::FieldEditor::Combo)
                {
                    const auto comboIt = comboFields.find(field.Id);
                    if (comboIt != comboFields.end())
                    {
                        const auto selected = comboIt->second.SelectedItem().try_as<ComboBoxItem>();
                            if (selected)
                        {
                            field.Value = ToUtf8(winrt::unbox_value<winrt::hstring>(selected.Content()).c_str());
                        }
                    }
                }
                else
                {
                    const auto textIt = textFields.find(field.Id);
                    if (textIt != textFields.end())
                    {
                        field.Value = ToUtf8(textIt->second.Text().c_str());
                    }
                }
            }
        }

        std::string errorMessage;
        if (!m_adapter->ApplyDetailEdits(*m_state.FocusedItemId, *details, errorMessage))
        {
            args.Cancel(true);
            ReportError(errorMessage);
        }
        else
        {
            m_statusText.Text(L"Properties updated.");
            Refresh();
        }

        deferral.Complete();
    });

    co_await dialog.ShowAsync();
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
    root.RowSpacing(12);

    auto contentRow = RowDefinition{};
    contentRow.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
    auto statusRow = RowDefinition{};
    statusRow.Height(GridLengthHelper::Auto());
    root.RowDefinitions().Append(contentRow);
    root.RowDefinitions().Append(statusRow);

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

    m_listView = ListView{};
    m_listView.SelectionMode(ListViewSelectionMode::Extended);
    m_listView.IsItemClickEnabled(true);
    m_listView.ItemClick([this](auto &&, auto &&) { UpdateSelectionFromList(); });
    m_listView.SelectionChanged([this](auto &&, auto &&) { UpdateSelectionFromList(); });
    listHost.Child(m_listView);

    m_layout.Children().Append(title);
    m_layout.Children().Append(subtitle);
    m_layout.Children().Append(m_actionsPanel);
    m_layout.Children().Append(listHost);

    Grid::SetRow(m_layout, 0);
    root.Children().Append(m_layout);

    Grid::SetRow(m_statusText, 1);
    root.Children().Append(m_statusText);

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
}

void ExplorerPage::RenderActions()
{
    m_actionsPanel.Children().Clear();

    auto refreshButton = Button{};
    refreshButton.Content(winrt::box_value(L"Refresh"));
    refreshButton.Click([this](auto &&, auto &&) { Refresh(); });
    m_actionsPanel.Children().Append(refreshButton);

    auto propertiesButton = Button{};
    propertiesButton.Content(winrt::box_value(L"Properties"));
    propertiesButton.IsEnabled(m_state.FocusedItemId.has_value());
    propertiesButton.Click([this](auto &&, auto &&) { ShowPropertiesDialogAsync(); });
    m_actionsPanel.Children().Append(propertiesButton);

    for (const auto &action : m_adapter->GetActions(m_state))
    {
        if (action.Id == "properties")
        {
            continue;
        }

        auto button = Button{};
        button.Content(winrt::box_value(winrt::hstring{ToWide(action.Label)}));
        button.IsEnabled(action.Enabled);
        button.Click([this, actionId = action.Id](auto &&, auto &&) { ExecuteAction(actionId); });
        m_actionsPanel.Children().Append(button);
    }
}

void ExplorerPage::RenderList()
{
    m_listView.Items().Clear();

    for (const auto &item : m_items)
    {
        const auto &descriptor = m_adapter->Describe();
        auto rowBorder = Border{};
        rowBorder.Padding(ThicknessHelper::FromLengths(10, 8, 10, 8));
        rowBorder.CornerRadius(CornerRadiusHelper::FromUniformRadius(6));
        rowBorder.Margin(ThicknessHelper::FromLengths(0, 0, 0, 2));

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

        ListViewItem container;
        container.Tag(winrt::box_value(winrt::hstring{ToWide(item.Id)}));
        container.Content(rowBorder);
        container.IsSelected(selected);
        m_listView.Items().Append(container);
    }
}

void ExplorerPage::UpdateSelectionFromList()
{
    m_state.SelectedItemIds.clear();
    const auto selected = m_listView.SelectedItems();
    for (uint32_t i = 0; i < selected.Size(); ++i)
    {
        const auto item = selected.GetAt(i).try_as<ListViewItem>();
        if (item)
        {
            m_state.SelectedItemIds.push_back(ToUtf8(winrt::unbox_value<winrt::hstring>(item.Tag()).c_str()));
        }
    }

    if (!m_state.SelectedItemIds.empty())
    {
        m_state.FocusedItemId = m_state.SelectedItemIds.front();
    }
    else
    {
        m_state.FocusedItemId.reset();
    }

    RenderActions();
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
