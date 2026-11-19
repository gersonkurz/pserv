#include "pch.h"
#include "properties_dialog.h"
#include <core/data_controller.h>
#include <core/data_object.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace pserv6
{
    PropertiesDialog::PropertiesDialog(pserv::DataController *pController, const std::vector<pserv::DataObject *> &objects, bool bReadOnly)
        : m_pController{pController},
          m_objects{objects},
          m_bReadOnly{bReadOnly}
    {
    }

    std::string PropertiesDialog::Title() const
    {
        const auto &objects = m_objects.Get();
        if (objects.size() == 1)
            return std::format("{} Properties: {}", m_pController->GetItemName(), objects.front()->GetItemName());
        return std::format("{} Properties ({} items)", m_pController->GetItemName(), objects.size());
    }

    pserv::DataObject *PropertiesDialog::CurrentObject() const
    {
        const auto &objects = m_objects.Get();
        return m_current < objects.size() ? objects[m_current] : nullptr;
    }

    std::string PropertiesDialog::ValueOf(size_t objectIndex, int column) const
    {
        if (const auto o = m_edits.find(objectIndex); o != m_edits.end())
        {
            if (const auto c = o->second.find(column); c != o->second.end())
                return c->second;
        }
        return m_objects.Get()[objectIndex]->GetProperty(column);
    }

    void PropertiesDialog::RecordEdit(size_t objectIndex, int column, std::string value)
    {
        if (value == m_objects.Get()[objectIndex]->GetProperty(column))
        {
            // Back to the original: no edit
            if (auto o = m_edits.find(objectIndex); o != m_edits.end())
            {
                o->second.erase(column);
                if (o->second.empty())
                    m_edits.erase(o);
            }
        }
        else
        {
            m_edits[objectIndex][column] = std::move(value);
        }
        // Actions run on the object as it is in the system: not while edits are pending
        if (m_actions)
            m_actions.Opacity(m_edits.empty() ? 1.0 : 0.4);
        if (m_actions)
        {
            for (auto const &child : m_actions.Children())
            {
                if (auto button = child.try_as<Button>())
                    button.IsEnabled(m_edits.empty());
            }
        }
    }

    void PropertiesDialog::Build(ContentDialog const &dialog)
    {
        m_dialog = make_weak(dialog);
        if (m_bReadOnly)
        {
            dialog.CloseButtonText(L"Close");
            dialog.DefaultButton(ContentDialogButton::Close);
        }
        else
        {
            dialog.PrimaryButtonText(L"OK");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(ContentDialogButton::Primary);
        }
        // Wider than the default 548 px: values like command lines need the room
        dialog.Resources().Insert(box_value(L"ContentDialogMaxWidth"), box_value(900.0));

        StackPanel root;
        root.Spacing(8);
        root.MinWidth(640);

        const auto &objects = m_objects.Get();
        if (objects.size() > 1)
        {
            ComboBox picker;
            picker.Header(box_value(L"Item"));
            picker.HorizontalAlignment(HorizontalAlignment::Stretch);
            for (auto *obj : objects)
                picker.Items().Append(box_value(to_hstring(obj->GetItemName())));
            picker.SelectedIndex(0);
            std::weak_ptr<PropertiesDialog> weak = shared_from_this();
            picker.SelectionChanged([weak](winrt::Windows::Foundation::IInspectable const &sender, auto &&)
                {
                    if (auto self = weak.lock())
                    {
                        const int index = sender.as<ComboBox>().SelectedIndex();
                        if (index >= 0)
                            self->ShowObject(static_cast<size_t>(index));
                    }
                });
            root.Children().Append(picker);
        }

        m_fields = StackPanel{};
        m_fields.Spacing(6);
        ScrollViewer scroller;
        scroller.Content(m_fields);
        scroller.MaxHeight(520);
        scroller.Padding(ThicknessHelper::FromLengths(0, 0, 16, 0));
        root.Children().Append(scroller);

        m_actions = StackPanel{};
        m_actions.Orientation(Orientation::Horizontal);
        m_actions.Spacing(8);
        root.Children().Append(m_actions);

        dialog.Content(root);
        ShowObject(0);
    }

    void PropertiesDialog::ShowObject(size_t index)
    {
        m_current = index;
        m_fields.Children().Clear();
        const auto &columns = m_pController->GetColumns();
        for (size_t c = 0; c < columns.size(); ++c)
            m_fields.Children().Append(MakeField(index, static_cast<int>(c)));

        // Action buttons for this object (visible in the properties dialog, available for it)
        m_actions.Children().Clear();
        if (m_bReadOnly)
            return;
        auto *pObject = m_objects.Get()[index];
        std::weak_ptr<PropertiesDialog> weak = shared_from_this();
        for (const auto *pAction : m_pController->GetActions(pObject))
        {
            const bool bVisible = (static_cast<int>(pAction->GetVisibility()) & static_cast<int>(pserv::ActionVisibility::PropertiesDialog)) != 0;
            if (!bVisible || pAction->IsSeparator() || pAction == &pserv::theDataPropertiesAction || !pAction->IsAvailableFor(pObject))
                continue;
            Button button;
            button.Content(box_value(to_hstring(pAction->GetName())));
            if (pAction->IsDestructive())
                button.Foreground(Application::Current().Resources().Lookup(box_value(L"SystemFillColorCriticalBrush")).as<Media::Brush>());
            button.IsEnabled(m_edits.empty());
            button.Click([weak, pAction](auto &&, auto &&)
                {
                    if (auto self = weak.lock())
                    {
                        // The owner runs it once this dialog has closed (one dialog at a time)
                        self->m_pRequestedAction = pAction;
                        if (auto dialog = self->m_dialog.get())
                            dialog.Hide();
                    }
                });
            m_actions.Children().Append(button);
        }
    }

    UIElement PropertiesDialog::MakeField(size_t objectIndex, int column)
    {
        const auto &col = m_pController->GetColumns()[static_cast<size_t>(column)];
        const std::string value = ValueOf(objectIndex, column);
        std::weak_ptr<PropertiesDialog> weak = shared_from_this();

        Grid row;
        ColumnDefinition labelColumn, valueColumn;
        labelColumn.Width(GridLengthHelper::FromPixels(190));
        valueColumn.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        row.ColumnDefinitions().Append(labelColumn);
        row.ColumnDefinitions().Append(valueColumn);
        TextBlock label;
        label.Text(to_hstring(col.DisplayName));
        label.VerticalAlignment(VerticalAlignment::Center);
        label.TextWrapping(TextWrapping::Wrap);
        if (col.Editable)
            label.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        row.Children().Append(label);

        FrameworkElement editor{nullptr};
        const auto type = col.Editable && !m_bReadOnly ? col.EditType : pserv::ColumnEditType::None;
        switch (type)
        {
        case pserv::ColumnEditType::Combo:
        {
            ComboBox combo;
            const auto options = m_pController->GetComboOptions(column);
            int selected = -1;
            for (size_t i = 0; i < options.size(); ++i)
            {
                combo.Items().Append(box_value(to_hstring(options[i])));
                if (options[i] == value)
                    selected = static_cast<int>(i);
            }
            combo.SelectedIndex(selected);
            combo.HorizontalAlignment(HorizontalAlignment::Stretch);
            combo.SelectionChanged([weak, objectIndex, column](winrt::Windows::Foundation::IInspectable const &sender, auto &&)
                {
                    if (auto self = weak.lock())
                    {
                        if (auto item = sender.as<ComboBox>().SelectedItem())
                            self->RecordEdit(objectIndex, column, to_string(unbox_value<hstring>(item)));
                    }
                });
            editor = combo;
            break;
        }
        case pserv::ColumnEditType::Integer:
        {
            NumberBox number;
            int initial = 0;
            std::from_chars(value.data(), value.data() + value.size(), initial);
            number.Value(initial);
            number.SpinButtonPlacementMode(NumberBoxSpinButtonPlacementMode::Inline);
            number.ValueChanged([weak, objectIndex, column](auto &&, NumberBoxValueChangedEventArgs const &args)
                {
                    if (auto self = weak.lock(); self && !std::isnan(args.NewValue()))
                        self->RecordEdit(objectIndex, column, std::to_string(static_cast<long long>(args.NewValue())));
                });
            editor = number;
            break;
        }
        case pserv::ColumnEditType::Text:
        case pserv::ColumnEditType::TextMultiline:
        {
            TextBox text;
            text.Text(to_hstring(value));
            if (type == pserv::ColumnEditType::TextMultiline)
            {
                text.AcceptsReturn(true);
                text.TextWrapping(TextWrapping::Wrap);
                text.MinHeight(80);
            }
            text.TextChanged([weak, objectIndex, column](winrt::Windows::Foundation::IInspectable const &sender, auto &&)
                {
                    if (auto self = weak.lock())
                        self->RecordEdit(objectIndex, column, to_string(sender.as<TextBox>().Text()));
                });
            editor = text;
            break;
        }
        default:
        {
            // Read-only: a selectable text box, so values can be copied
            TextBox text;
            text.Text(to_hstring(value));
            text.IsReadOnly(true);
            text.PlaceholderText(L"N/A");
            if (value.find('\n') != std::string::npos)
            {
                text.AcceptsReturn(true);
                text.TextWrapping(TextWrapping::Wrap);
                text.MaxHeight(120);
            }
            text.BorderThickness(ThicknessHelper::FromUniformLength(0));
            text.Background(Media::SolidColorBrush{winrt::Microsoft::UI::Colors::Transparent()});
            editor = text;
            break;
        }
        }
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(editor, to_hstring(col.DisplayName));
        Grid::SetColumn(editor, 1);
        row.Children().Append(editor);
        return row;
    }

    PropertiesDialog::ApplyOutcome PropertiesDialog::ApplyEdits()
    {
        ApplyOutcome outcome;
        for (const auto &[index, columnEdits] : m_edits)
        {
            auto *pObject = m_objects.Get()[index];
            m_pController->BeginPropertyEdits(pObject);
            bool bAccepted = true;
            for (const auto &[column, value] : columnEdits)
            {
                if (!m_pController->SetPropertyEdit(pObject, column, value))
                {
                    outcome.errors.push_back(std::format("{}: '{}' was not accepted for {}", pObject->GetItemName(), value,
                        m_pController->GetColumns()[static_cast<size_t>(column)].DisplayName));
                    bAccepted = false;
                }
            }
            if (!bAccepted)
                continue; // no partial commit of an object's edits
            if (m_pController->CommitPropertyEdits(pObject))
                outcome.bChanged = true;
            else
                outcome.errors.push_back(std::format("{}: the changes could not be applied (see the log)", pObject->GetItemName()));
        }
        return outcome;
    }
} // namespace pserv6
