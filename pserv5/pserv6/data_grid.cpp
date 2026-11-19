#include "pch.h"
#include "data_grid.h"
#include <core/data_object.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Media;

namespace pserv6
{
    namespace
    {
        // One list item per DataObject. It holds a reference, so a publish that drops the object
        // from the controller cannot leave the grid with a dangling pointer.
        struct RowItem : implements<RowItem, winrt::Windows::Foundation::IStringable>
        {
            explicit RowItem(pserv::DataObject *p) : m_p{p} { m_p->Retain(REFCOUNT_DEBUG_ARGS); }
            ~RowItem() { m_p->Release(REFCOUNT_DEBUG_ARGS); }
            hstring ToString() { return to_hstring(m_p->GetItemName()); } // UI Automation name
            pserv::DataObject *m_p;
        };

        pserv::DataObject *ObjectOf(winrt::Windows::Foundation::IInspectable const &item)
        {
            return get_self<RowItem>(item.as<winrt::Windows::Foundation::IStringable>())->m_p;
        }

        double DefaultWidth(const pserv::DataObjectColumn &column, size_t index)
        {
            if (index == 0)
                return 240.0;
            return column.GetAlignment() == pserv::ColumnAlignment::Right ? 110.0 : 190.0;
        }
    } // namespace

    void DataGrid::Attach(ListView const &list, Grid const &header)
    {
        m_list = list;
        m_header = header;
        auto res = Application::Current().Resources();
        m_normalBrush = res.Lookup(box_value(L"TextFillColorPrimaryBrush")).as<Brush>();
        m_disabledBrush = res.Lookup(box_value(L"TextFillColorDisabledBrush")).as<Brush>();
        m_highlightBrush = res.Lookup(box_value(L"AccentTextFillColorPrimaryBrush")).as<Brush>();
    }

    void DataGrid::SetController(pserv::DataController *pController)
    {
        m_pController = pController;
        m_widths.clear();
        if (m_pController)
        {
            const auto &columns = m_pController->GetColumns();
            for (size_t i = 0; i < columns.size(); ++i)
                m_widths.push_back(DefaultWidth(columns[i], i));
        }
        ++m_layoutGeneration;
        BuildHeader();

        // New collection in one go (no per-item change notifications)
        std::vector<winrt::Windows::Foundation::IInspectable> items;
        m_current.clear();
        if (m_pController)
        {
            for (auto *obj : m_pController->GetDataObjects())
            {
                if (m_filter.empty() || obj->MatchesFilter(m_filter))
                {
                    items.push_back(make<RowItem>(obj));
                    m_current.push_back(obj);
                }
            }
        }
        m_items = single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>(std::move(items));
        m_list.ItemsSource(m_items);
    }

    void DataGrid::SetFilter(std::string lowerCaseFilter)
    {
        m_filter = std::move(lowerCaseFilter);
        Refresh();
    }

    void DataGrid::BuildHeader()
    {
        m_header.Children().Clear();
        m_header.ColumnDefinitions().Clear();
        if (!m_pController)
            return;
        const auto &columns = m_pController->GetColumns();
        double total = 0;
        for (size_t c = 0; c < columns.size(); ++c)
        {
            ColumnDefinition cd;
            cd.Width(GridLengthHelper::FromPixels(m_widths[c]));
            m_header.ColumnDefinitions().Append(cd);

            Button button;
            TextBlock label;
            label.Text(to_hstring(columns[c].DisplayName));
            label.TextTrimming(TextTrimming::CharacterEllipsis);
            label.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            button.Content(label);
            button.HorizontalAlignment(HorizontalAlignment::Stretch);
            button.HorizontalContentAlignment(columns[c].GetAlignment() == pserv::ColumnAlignment::Right ? HorizontalAlignment::Right : HorizontalAlignment::Left);
            button.Padding(ThicknessHelper::FromLengths(8, 0, 8, 0));
            button.Background(SolidColorBrush{winrt::Microsoft::UI::Colors::Transparent()});
            button.BorderThickness(ThicknessHelper::FromUniformLength(0));
            Grid::SetColumn(button, static_cast<int>(c));
            const int column = static_cast<int>(c);
            button.Click([this, column](auto &&, auto &&) { SortBy(column); });
            m_header.Children().Append(button);
            total += m_widths[c];
        }
        m_header.Width(total);
        m_list.Width(total + 24);
    }

    void DataGrid::SortBy(int column)
    {
        if (!m_pController)
            return;
        // Same column again toggles the direction
        const bool bAscending = m_pController->GetLastSortColumn() == column ? !m_pController->IsLastSortAscending() : true;
        m_pController->Sort(column, bAscending);
        Refresh();
        if (m_onSortChanged)
            m_onSortChanged();
    }

    void DataGrid::Refresh()
    {
        if (!m_pController || !m_items)
            return;

        // Target order: the controller's (sorted) live objects that pass the filter
        std::vector<pserv::DataObject *> next;
        next.reserve(m_pController->GetDataObjects().GetSize());
        for (auto *obj : m_pController->GetDataObjects())
        {
            if (m_filter.empty() || obj->MatchesFilter(m_filter))
                next.push_back(obj);
        }

        const auto start = std::chrono::steady_clock::now();
        if (next != m_current)
        {
            // The common refresh (a few processes come and go, order of the rest unchanged) is
            // applied as individual removes/inserts, so the ListView keeps scroll position and
            // selection. Anything else (first population, re-sort, filter change, many changes)
            // becomes one ReplaceAll: one Reset notification instead of thousands of events.
            constexpr size_t maxIncrementalChanges = 64;
            std::unordered_set<pserv::DataObject *> nextSet(next.begin(), next.end());
            std::unordered_set<pserv::DataObject *> currentSet(m_current.begin(), m_current.end());
            std::vector<pserv::DataObject *> currentKept, nextOld;
            for (auto *p : m_current)
                if (nextSet.contains(p))
                    currentKept.push_back(p);
            for (auto *p : next)
                if (currentSet.contains(p))
                    nextOld.push_back(p);
            const size_t changes = (m_current.size() - currentKept.size()) + (next.size() - nextOld.size());
            if (currentKept == nextOld && changes <= maxIncrementalChanges && !m_current.empty())
            {
                for (size_t i = m_current.size(); i-- > 0;)
                {
                    if (!nextSet.contains(m_current[i]))
                    {
                        m_items.RemoveAt(static_cast<uint32_t>(i));
                        m_current.erase(m_current.begin() + static_cast<ptrdiff_t>(i));
                    }
                }
                for (size_t i = 0; i < next.size(); ++i)
                {
                    if (i < m_current.size() && m_current[i] == next[i])
                        continue;
                    m_items.InsertAt(static_cast<uint32_t>(i), make<RowItem>(next[i])); // new: old ones keep their order
                    m_current.insert(m_current.begin() + static_cast<ptrdiff_t>(i), next[i]);
                }
            }
            else
            {
                // Reuse the existing items of kept objects (no new RowItem, no extra Retain).
                std::unordered_map<pserv::DataObject *, winrt::Windows::Foundation::IInspectable> existing;
                existing.reserve(m_current.size());
                for (uint32_t i = 0; i < m_current.size(); ++i)
                    existing.emplace(m_current[i], m_items.GetAt(i));
                std::vector<winrt::Windows::Foundation::IInspectable> items;
                items.reserve(next.size());
                for (auto *p : next)
                {
                    const auto it = existing.find(p);
                    items.push_back(it != existing.end() ? it->second : make<RowItem>(p));
                }
                m_items.ReplaceAll(items);
                m_current = std::move(next);
                ++m_stats.resets;
            }
        }
        UpdateRealized();
        m_stats.refreshMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        ++m_stats.refreshes;
    }

    void DataGrid::OnContainerContentChanging(ContainerContentChangingEventArgs const &args)
    {
        if (args.InRecycleQueue() || !m_pController)
            return;
        const auto start = std::chrono::steady_clock::now();
        auto row = args.ItemContainer().ContentTemplateRoot().as<Grid>();
        const auto stamp = row.Tag() ? unbox_value<int>(row.Tag()) : -1;
        if (stamp != m_layoutGeneration)
        {
            // (Re)build the cells for the current column layout
            row.Children().Clear();
            row.ColumnDefinitions().Clear();
            const auto &columns = m_pController->GetColumns();
            for (size_t c = 0; c < columns.size(); ++c)
            {
                ColumnDefinition cd;
                cd.Width(GridLengthHelper::FromPixels(m_widths[c]));
                row.ColumnDefinitions().Append(cd);
                TextBlock cell;
                cell.VerticalAlignment(VerticalAlignment::Center);
                cell.TextTrimming(TextTrimming::CharacterEllipsis);
                cell.Padding(ThicknessHelper::FromLengths(8, 0, 8, 0));
                cell.TextAlignment(columns[c].GetAlignment() == pserv::ColumnAlignment::Right ? TextAlignment::Right : TextAlignment::Left);
                Grid::SetColumn(cell, static_cast<int>(c));
                row.Children().Append(cell);
            }
            row.Tag(box_value(m_layoutGeneration));
        }
        FillRow(row, ObjectOf(args.Item()));
        args.Handled(true);
        m_stats.containerMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        ++m_stats.containers;
    }

    void DataGrid::FillRow(Grid const &row, pserv::DataObject *pObject)
    {
        const auto state = m_pController->GetVisualState(pObject);
        const Brush &brush = state == pserv::VisualState::Highlighted ? m_highlightBrush : (state == pserv::VisualState::Disabled ? m_disabledBrush : m_normalBrush);
        auto cells = row.Children();
        for (uint32_t c = 0; c < cells.Size(); ++c)
        {
            auto cell = cells.GetAt(c).as<TextBlock>();
            hstring text = to_hstring(pObject->GetProperty(static_cast<int>(c)));
            if (cell.Text() != text)
                cell.Text(text);
            if (cell.Foreground() != brush)
                cell.Foreground(brush);
        }
    }

    void DataGrid::UpdateRealized()
    {
        // Only the realized range (viewport + cache) has containers; everything else is filled on
        // demand when it scrolls into view.
        auto panel = m_list.ItemsPanelRoot().try_as<ItemsStackPanel>();
        if (!panel || panel.FirstCacheIndex() < 0)
            return;
        for (int i = panel.FirstCacheIndex(); i <= panel.LastCacheIndex() && i < static_cast<int>(m_current.size()); ++i)
        {
            auto container = m_list.ContainerFromIndex(i).try_as<ListViewItem>();
            if (!container)
                continue;
            auto row = container.ContentTemplateRoot().try_as<Grid>();
            if (!row || !row.Tag() || unbox_value<int>(row.Tag()) != m_layoutGeneration)
                continue;
            FillRow(row, m_current[static_cast<size_t>(i)]);
        }
    }

    std::vector<pserv::DataObject *> DataGrid::GetSelection() const
    {
        std::vector<pserv::DataObject *> selection;
        for (auto const &range : m_list.SelectedRanges())
        {
            for (int i = range.FirstIndex(); i <= range.LastIndex() && i < static_cast<int>(m_current.size()); ++i)
                selection.push_back(m_current[static_cast<size_t>(i)]);
        }
        return selection;
    }
} // namespace pserv6
