#include "pch.h"
#include "data_grid.h"
#include <config/settings.h>
#include <core/data_object.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Input;
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
            /// The row shows another object from now on (a refresh that changes the order rewrites
            /// the rows in place instead of resetting the list).
            void Set(pserv::DataObject *p)
            {
                p->Retain(REFCOUNT_DEBUG_ARGS);
                m_p->Release(REFCOUNT_DEBUG_ARGS);
                m_p = p;
            }
            hstring ToString() { return to_hstring(m_p->GetItemName()); } // UI Automation name
            pserv::DataObject *m_p;
        };

        pserv::DataObject *ObjectOf(winrt::Windows::Foundation::IInspectable const &item)
        {
            return get_self<RowItem>(item.as<winrt::Windows::Foundation::IStringable>())->m_p;
        }

        constexpr double MinColumnWidth = 30.0;
        constexpr double MaxColumnWidth = 2000.0;

        std::vector<std::string> SplitList(const std::string &text)
        {
            std::vector<std::string> parts;
            std::string item;
            std::istringstream in{text};
            while (std::getline(in, item, ','))
                parts.push_back(item);
            return parts;
        }

        bool ParseInt(const std::string &text, int &value)
        {
            const auto *begin = text.data();
            const auto *end = begin + text.size();
            while (begin < end && *begin == ' ')
                ++begin;
            const auto result = std::from_chars(begin, end, value);
            return result.ec == std::errc{} && result.ptr == end;
        }

        ScrollViewer FindScrollViewer(DependencyObject const &root)
        {
            const int n = VisualTreeHelper::GetChildrenCount(root);
            for (int i = 0; i < n; ++i)
            {
                auto child = VisualTreeHelper::GetChild(root, i);
                if (auto sv = child.try_as<ScrollViewer>())
                    return sv;
                if (auto sv = FindScrollViewer(child))
                    return sv;
            }
            return nullptr;
        }
    } // namespace

    void DataGrid::Attach(ListView const &list, Grid const &header, ScrollViewer const &headerScroller)
    {
        m_list = list;
        m_header = header;
        m_headerScroller = headerScroller;
        // The list's own ScrollViewer exists once its template is applied
        m_list.Loaded([this](auto &&, auto &&) { Scroller(); });
    }

    void DataGrid::SetBrushes(const Brushes &brushes)
    {
        m_normalBrush = brushes.normal;
        m_highlightBrush = brushes.highlight;
        m_disabledBrush = brushes.disabled;
        m_gripperBrush = brushes.gripper;
        m_dropBrush = brushes.hover;
        m_alternateBrush = brushes.alternate;
        if (m_pController)
        {
            BuildHeader();    // grippers and labels
            UpdateRealized(); // FillRow replaces the cells' brushes
        }
    }

    void DataGrid::SetZoom(double zoom)
    {
        zoom = std::clamp(zoom, 0.5, 2.0);
        if (zoom == m_zoom)
            return;
        m_zoom = zoom;
        if (!m_pController)
            return;
        ++m_layoutGeneration; // every row rebuilds its cells with the new font size
        BuildHeader();
        UpdateRealized();
    }

    void DataGrid::SetFontFamily(const std::wstring &name)
    {
        m_fontFamily = name.empty() ? nullptr : FontFamily{name};
        if (!m_pController)
            return;
        ++m_layoutGeneration; // every row rebuilds its cells with the new font
        BuildHeader();
        UpdateRealized();
    }

    void DataGrid::ApplyRowHeight(Primitives::SelectorItem const &container) const
    {
        const double height = RowHeight();
        if (container.Height() != height)
        {
            container.MinHeight(height);
            container.Height(height);
        }
    }

    ScrollViewer DataGrid::Scroller()
    {
        if (!m_scroller)
        {
            m_scroller = FindScrollViewer(m_list);
            if (m_scroller)
            {
                // The header follows the rows horizontally. The handler takes the scroller from
                // the sender (it must not capture the element it is attached to).
                m_scroller.ViewChanged([this](winrt::Windows::Foundation::IInspectable const &sender, auto &&)
                    {
                        const double offset = sender.as<ScrollViewer>().HorizontalOffset();
                        if (m_headerScroller.HorizontalOffset() != offset)
                            m_headerScroller.ChangeView(offset, nullptr, nullptr, true);
                    });
            }
        }
        return m_scroller;
    }

    // --- Column layout and its persistence -------------------------------------------------

    void DataGrid::LoadLayout()
    {
        m_widths.clear();
        m_fullOrder.clear();
        m_hidden.clear();
        m_order.clear();
        if (!m_pController)
            return;
        const size_t n = m_pController->GetColumns().size();
        auto *pSection = pserv::config::theSettings.getSectionFor(std::string{m_pController->GetControllerName()});

        // Widths by column index; a missing or invalid entry falls back to a default.
        const auto widths = pSection ? SplitList(pSection->columnWidths.get()) : std::vector<std::string>{};
        for (size_t c = 0; c < n; ++c)
        {
            int w = 0;
            const bool bValid = c < widths.size() && ParseInt(widths[c], w) && w > 0;
            m_widths.push_back(bValid ? std::clamp(static_cast<double>(w), MinColumnWidth, MaxColumnWidth) : (c == 0 ? 240.0 : 150.0));
        }

        // Display position by column index (pserv5's format); used only if it is a permutation.
        std::vector<int> position(n, -1);
        bool bValidOrder = pSection != nullptr;
        if (bValidOrder)
        {
            const auto order = SplitList(pSection->columnOrder.get());
            std::vector<bool> used(n, false);
            for (size_t c = 0; c < n && bValidOrder; ++c)
            {
                int p = -1;
                bValidOrder = c < order.size() && ParseInt(order[c], p) && p >= 0 && p < static_cast<int>(n) && !used[static_cast<size_t>(p)];
                if (bValidOrder)
                {
                    used[static_cast<size_t>(p)] = true;
                    position[c] = p;
                }
            }
        }
        m_fullOrder.assign(n, 0);
        for (size_t c = 0; c < n; ++c)
            m_fullOrder[bValidOrder ? static_cast<size_t>(position[c]) : c] = static_cast<int>(c);

        // Hidden columns, as a list of column indices
        m_hidden.assign(n, false);
        for (const auto &entry : pSection ? SplitList(pSection->columnHidden.get()) : std::vector<std::string>{})
        {
            int c = -1;
            if (ParseInt(entry, c) && c >= 0 && c < static_cast<int>(n))
                m_hidden[static_cast<size_t>(c)] = true;
        }
        RebuildShown();

        // Sort: restore it unless the controller already has one (it remembers it across visits)
        if (pSection && m_pController->GetLastSortColumn() < 0)
        {
            const int sortColumn = pSection->sortColumn.get();
            if (sortColumn >= 0 && sortColumn < static_cast<int>(n))
                m_pController->Sort(sortColumn, pSection->sortAscending.get());
        }
    }

    void DataGrid::SaveLayout() const
    {
        if (!m_pController)
            return;
        auto *pSection = pserv::config::theSettings.getSectionFor(std::string{m_pController->GetControllerName()});
        if (!pSection)
            return;
        std::string widths, order;
        std::vector<int> position(m_fullOrder.size(), 0);
        for (size_t d = 0; d < m_fullOrder.size(); ++d)
            position[static_cast<size_t>(m_fullOrder[d])] = static_cast<int>(d);
        for (size_t c = 0; c < m_widths.size(); ++c)
        {
            widths += std::format("{}{}", c ? "," : "", static_cast<int>(std::lround(m_widths[c])));
            order += std::format("{}{}", c ? "," : "", position[c]);
        }
        pSection->columnWidths.set(widths);
        pSection->columnOrder.set(order);
        std::string hidden;
        for (size_t c = 0; c < m_hidden.size(); ++c)
            if (m_hidden[c])
                hidden += std::format("{}{}", hidden.empty() ? "" : ",", c);
        pSection->columnHidden.set(hidden);
        pSection->sortColumn.set(m_pController->GetLastSortColumn());
        pSection->sortAscending.set(m_pController->IsLastSortAscending());
    }

    void DataGrid::RebuildShown()
    {
        m_order.clear();
        for (const int c : m_fullOrder)
            if (!m_hidden[static_cast<size_t>(c)])
                m_order.push_back(c);
        if (m_order.empty() && !m_fullOrder.empty()) // never an empty table
        {
            m_hidden[static_cast<size_t>(m_fullOrder.front())] = false;
            m_order.push_back(m_fullOrder.front());
        }
    }

    void DataGrid::LayoutChanged()
    {
        RebuildShown();
        ++m_layoutGeneration;
        BuildHeader();
        UpdateRealized(); // realized rows rebuild their cells for the new layout
        SaveLayout();
        if (m_onHeaderChanged)
            m_onHeaderChanged(); // the Sort and Columns menus list the columns
    }

    void DataGrid::SetColumnHidden(int column, bool bHidden)
    {
        if (column < 0 || column >= static_cast<int>(m_hidden.size()) || m_hidden[static_cast<size_t>(column)] == bHidden)
            return;
        m_hidden[static_cast<size_t>(column)] = bHidden;
        LayoutChanged();
    }

    void DataGrid::ResetLayout()
    {
        if (!m_pController)
            return;
        if (auto *pSection = pserv::config::theSettings.getSectionFor(std::string{m_pController->GetControllerName()}))
        {
            pSection->columnWidths.revertToDefault();
            pSection->columnOrder.revertToDefault();
            pSection->columnHidden.revertToDefault();
        }
        LoadLayout();
        LayoutChanged();
    }

    void DataGrid::SetController(pserv::DataController *pController)
    {
        m_pController = pController;
        LoadLayout();
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
        if (m_onHeaderChanged)
            m_onHeaderChanged(); // the columns (and maybe the sort) changed
    }

    void DataGrid::SetFilter(std::string lowerCaseFilter)
    {
        m_filter = std::move(lowerCaseFilter);
        Refresh();
    }

    // --- Header: sort (click), reorder (drag a header), resize (drag a gripper) -------------

    void DataGrid::BuildHeader()
    {
        m_header.Children().Clear();
        m_header.ColumnDefinitions().Clear();
        m_sortGlyphs.clear();
        m_headerDrag = {};
        m_resize = {};
        if (!m_pController)
            return;
        const auto &columns = m_pController->GetColumns();
        double total = 0;
        for (size_t d = 0; d < m_order.size(); ++d)
        {
            const int c = m_order[d];
            const auto &column = columns[static_cast<size_t>(c)];
            const bool bRight = column.GetAlignment() == pserv::ColumnAlignment::Right;

            ColumnDefinition cd;
            cd.Width(GridLengthHelper::FromPixels(m_widths[static_cast<size_t>(c)] * m_zoom));
            m_header.ColumnDefinitions().Append(cd);

            Grid cell;
            Grid::SetColumn(cell, static_cast<int>(d));

            // Label plus sort glyph
            Grid content;
            ColumnDefinition labelColumn, glyphColumn;
            labelColumn.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            glyphColumn.Width(GridLengthHelper::Auto());
            content.ColumnDefinitions().Append(labelColumn);
            content.ColumnDefinitions().Append(glyphColumn);
            TextBlock label;
            label.Text(to_hstring(column.DisplayName));
            label.FontSize(FontSize());
            if (m_fontFamily)
                label.FontFamily(m_fontFamily);
            label.TextTrimming(TextTrimming::CharacterEllipsis);
            label.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
            label.TextAlignment(bRight ? TextAlignment::Right : TextAlignment::Left);
            content.Children().Append(label);
            TextBlock glyph;
            glyph.FontSize(9 * m_zoom);
            glyph.Margin(ThicknessHelper::FromLengths(4, 0, 0, 0));
            glyph.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(glyph, 1);
            content.Children().Append(glyph);
            m_sortGlyphs.push_back(glyph);

            // The header cell handles the pointer itself, so one element decides between click and
            // drag (no Click event to suppress after a drag): it captures the pointer on press; a
            // release without moving more than 8 px sorts, otherwise the column moves to where it
            // was dropped. Keyboard sorting: the toolbar's Sort menu (MainWindow).
            content.Padding(ThicknessHelper::FromLengths(8, 0, 10, 0));
            content.VerticalAlignment(VerticalAlignment::Center);
            cell.Children().Append(content);
            cell.Background(SolidColorBrush{winrt::Microsoft::UI::Colors::Transparent()}); // hit-testable
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(cell, to_hstring(std::format("Sort by {}", column.DisplayName)));
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetAccessibilityView(cell, winrt::Microsoft::UI::Xaml::Automation::Peers::AccessibilityView::Content);
            // Lifetime: the handlers are owned by the elements they are attached to, so they must
            // not capture those elements (a strong reference cycle would keep every discarded
            // header alive); they take them from the event's sender. `this` (the grid) outlives
            // its header.
            const int position = static_cast<int>(d);
            cell.PointerEntered([this](winrt::Windows::Foundation::IInspectable const &sender, auto &&)
                {
                    if (m_headerDrag.fromPosition < 0)
                        sender.as<Grid>().Background(m_dropBrush);
                });
            cell.PointerExited([this, position](winrt::Windows::Foundation::IInspectable const &sender, auto &&)
                {
                    if (m_headerDrag.fromPosition != position)
                        sender.as<Grid>().Background(SolidColorBrush{winrt::Microsoft::UI::Colors::Transparent()});
                });
            cell.PointerPressed([this, position](winrt::Windows::Foundation::IInspectable const &sender, PointerRoutedEventArgs const &e)
                {
                    auto self = sender.as<Grid>();
                    if (!e.GetCurrentPoint(self).Properties().IsLeftButtonPressed())
                        return;
                    m_headerDrag = {position, e.GetCurrentPoint(m_header).Position().X, false};
                    const bool bCaptured = self.CapturePointer(e.Pointer());
                    spdlog::debug("header: press on position {} (captured: {})", position, bCaptured);
                    e.Handled(true);
                });
            cell.PointerMoved([this, position](winrt::Windows::Foundation::IInspectable const &, PointerRoutedEventArgs const &e)
                {
                    if (m_headerDrag.fromPosition != position)
                        return;
                    if (!m_headerDrag.bMoved && std::abs(e.GetCurrentPoint(m_header).Position().X - m_headerDrag.startX) > 8)
                        m_headerDrag.bMoved = true;
                    e.Handled(true); // keep the scroll viewer around the header out of this drag
                });
            cell.PointerReleased([this, c, position](winrt::Windows::Foundation::IInspectable const &sender, PointerRoutedEventArgs const &e)
                {
                    spdlog::debug("header: release on position {} (drag from {}, moved {})", position, m_headerDrag.fromPosition, m_headerDrag.bMoved);
                    if (m_headerDrag.fromPosition != position)
                        return;
                    const auto drag = std::exchange(m_headerDrag, {});
                    sender.as<Grid>().ReleasePointerCaptures();
                    e.Handled(true);
                    if (drag.bMoved)
                    {
                        // Rebuild the header after this event, not inside the cell's own input
                        const int target = DisplayPositionAt(e.GetCurrentPoint(m_header).Position().X);
                        m_list.DispatcherQueue().TryEnqueue([this, from = drag.fromPosition, target] { MoveColumn(from, target); });
                    }
                    else
                    {
                        SortBy(c);
                    }
                });
            cell.PointerCaptureLost([this, position](winrt::Windows::Foundation::IInspectable const &sender, auto &&)
                {
                    if (m_headerDrag.fromPosition == position)
                    {
                        spdlog::debug("header: capture lost on position {} during a press", position);
                        m_headerDrag = {};
                    }
                    sender.as<Grid>().Background(SolidColorBrush{winrt::Microsoft::UI::Colors::Transparent()});
                });

            // Resize gripper at the right edge (a Border whose child is the visible line)
            Border gripper;
            gripper.Width(6);
            gripper.HorizontalAlignment(HorizontalAlignment::Right);
            gripper.Background(SolidColorBrush{winrt::Microsoft::UI::Colors::Transparent()});
            Border line;
            line.Width(1);
            line.Margin(ThicknessHelper::FromLengths(0, 6, 0, 6));
            line.HorizontalAlignment(HorizontalAlignment::Right);
            line.Background(m_gripperBrush);
            line.Opacity(0.5);
            gripper.Child(line);
            auto lineOf = [](winrt::Windows::Foundation::IInspectable const &sender) { return sender.as<Border>().Child().as<Border>(); };
            gripper.PointerEntered([lineOf](winrt::Windows::Foundation::IInspectable const &sender, auto &&)
                {
                    auto l = lineOf(sender);
                    l.Opacity(1.0);
                    l.Width(3);
                });
            gripper.PointerExited([this, lineOf](winrt::Windows::Foundation::IInspectable const &sender, auto &&)
                {
                    if (m_resize.column < 0)
                    {
                        auto l = lineOf(sender);
                        l.Opacity(0.5);
                        l.Width(1);
                    }
                });
            gripper.PointerPressed([this, c](winrt::Windows::Foundation::IInspectable const &sender, PointerRoutedEventArgs const &e)
                {
                    m_resize = {c, e.GetCurrentPoint(m_header).Position().X, m_widths[static_cast<size_t>(c)] * m_zoom};
                    sender.as<Border>().CapturePointer(e.Pointer());
                    e.Handled(true);
                });
            gripper.PointerMoved([this](winrt::Windows::Foundation::IInspectable const &, PointerRoutedEventArgs const &e)
                {
                    if (m_resize.column < 0)
                        return;
                    const double width = m_resize.startWidth + e.GetCurrentPoint(m_header).Position().X - m_resize.startX;
                    SetColumnWidth(m_resize.column, width / m_zoom); // stored widths are unzoomed
                    e.Handled(true);
                });
            auto endResize = [this, lineOf](winrt::Windows::Foundation::IInspectable const &sender, PointerRoutedEventArgs const &e)
            {
                if (m_resize.column < 0)
                    return;
                m_resize = {};
                sender.as<Border>().ReleasePointerCaptures();
                auto l = lineOf(sender);
                l.Opacity(0.5);
                l.Width(1);
                // Containers waiting in the recycle queue still have the old widths: a new
                // generation makes them rebuild; realized rows are re-stamped as current.
                ++m_layoutGeneration;
                ApplyWidthsToRealizedRows();
                SaveLayout();
                e.Handled(true);
            };
            // Double-click: fit the column to its content (as Explorer and Excel do)
            gripper.DoubleTapped([this, c](auto &&, DoubleTappedRoutedEventArgs const &e)
                {
                    m_resize = {}; // the press of the second click must not drag the width back
                    AutoSizeColumn(c);
                    e.Handled(true);
                });
            gripper.PointerReleased(endResize);
            gripper.PointerCaptureLost(endResize);
            cell.Children().Append(gripper);

            m_header.Children().Append(cell);
            total += m_widths[static_cast<size_t>(c)] * m_zoom;
        }
        m_header.Width(total);
        m_header.Height(std::round(30.0 * m_zoom));
        UpdateSortIndicators();
    }

    void DataGrid::UpdateSortIndicators()
    {
        if (!m_pController)
            return;
        const int sortColumn = m_pController->GetLastSortColumn();
        for (size_t d = 0; d < m_sortGlyphs.size() && d < m_order.size(); ++d)
        {
            const bool bSorted = m_order[d] == sortColumn;
            m_sortGlyphs[d].Text(bSorted ? (m_pController->IsLastSortAscending() ? L"\u25B2" : L"\u25BC") : L"");
        }
    }

    int DataGrid::DisplayPositionAt(double x) const
    {
        double right = 0;
        for (size_t d = 0; d < m_order.size(); ++d)
        {
            right += m_widths[static_cast<size_t>(m_order[d])] * m_zoom;
            if (x < right)
                return static_cast<int>(d);
        }
        return m_order.empty() ? 0 : static_cast<int>(m_order.size()) - 1;
    }

    void DataGrid::MoveColumn(int from, int to)
    {
        const int n = static_cast<int>(m_order.size());
        if (from < 0 || from >= n || to < 0 || to >= n || from == to)
            return;
        // Positions among the shown columns -> positions in the full order (hidden ones keep theirs)
        const auto fullPosition = [this](int shown) { return std::find(m_fullOrder.begin(), m_fullOrder.end(), m_order[static_cast<size_t>(shown)]) - m_fullOrder.begin(); };
        const auto fullFrom = fullPosition(from), fullTo = fullPosition(to);
        const int column = m_fullOrder[static_cast<size_t>(fullFrom)];
        m_fullOrder.erase(m_fullOrder.begin() + fullFrom);
        m_fullOrder.insert(m_fullOrder.begin() + fullTo, column);
        LayoutChanged();
    }

    void DataGrid::SetColumnWidth(int column, double width)
    {
        if (column < 0 || column >= static_cast<int>(m_widths.size()))
            return;
        width = std::clamp(width, MinColumnWidth, MaxColumnWidth);
        m_widths[static_cast<size_t>(column)] = width;
        double total = 0;
        for (size_t d = 0; d < m_order.size(); ++d)
        {
            const double w = m_widths[static_cast<size_t>(m_order[d])] * m_zoom;
            total += w;
            if (m_order[d] == column && d < m_header.ColumnDefinitions().Size())
                m_header.ColumnDefinitions().GetAt(static_cast<uint32_t>(d)).Width(GridLengthHelper::FromPixels(w));
        }
        m_header.Width(total);
        ApplyWidthsToRealizedRows();
        if (m_resize.column < 0) // programmatic (not a drag in progress): persist now
        {
            ++m_layoutGeneration;
            ApplyWidthsToRealizedRows();
            SaveLayout();
        }
    }

    void DataGrid::AutoSizeColumn(int column)
    {
        if (!m_pController || column < 0 || column >= static_cast<int>(m_widths.size()))
            return;
        // ponytail: measures the header and the 30 longest texts by length, not every row (a
        // proportional font can make a shorter text wider; measuring 15,000 rows takes too long).
        // Measure all rows if a column ever comes out visibly too narrow.
        std::vector<std::string> texts;
        texts.reserve(m_current.size());
        for (const auto *obj : m_current)
            texts.push_back(obj->GetProperty(column));
        const size_t count = std::min<size_t>(texts.size(), 30);
        std::partial_sort(texts.begin(), texts.begin() + static_cast<ptrdiff_t>(count), texts.end(),
            [](const std::string &a, const std::string &b) { return a.size() > b.size(); });

        TextBlock probe;
        probe.FontSize(FontSize());
        if (m_fontFamily)
            probe.FontFamily(m_fontFamily);
        const auto measure = [&probe](const std::string &text)
        {
            probe.Text(to_hstring(text));
            probe.Measure({std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()});
            return static_cast<double>(probe.DesiredSize().Width);
        };
        double width = 0;
        for (size_t i = 0; i < count; ++i)
            width = std::max(width, measure(texts[i]));
        width += 18; // the cells' padding
        probe.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        width = std::max(width, measure(m_pController->GetColumns()[static_cast<size_t>(column)].DisplayName) + 20 + 14 * m_zoom); // padding, sort marker
        SetColumnWidth(column, width / m_zoom); // stored widths are unzoomed
    }

    void DataGrid::ApplyWidthsToRealizedRows()
    {
        auto panel = m_list.ItemsPanelRoot().try_as<ItemsStackPanel>();
        if (!panel || panel.FirstCacheIndex() < 0)
            return;
        for (int i = panel.FirstCacheIndex(); i <= panel.LastCacheIndex(); ++i)
        {
            auto container = m_list.ContainerFromIndex(i).try_as<ListViewItem>();
            if (!container)
                continue;
            auto row = container.ContentTemplateRoot().try_as<Grid>();
            if (!row || row.ColumnDefinitions().Size() != m_order.size())
                continue;
            for (uint32_t d = 0; d < m_order.size(); ++d)
                row.ColumnDefinitions().GetAt(d).Width(GridLengthHelper::FromPixels(m_widths[static_cast<size_t>(m_order[d])] * m_zoom));
            row.Tag(box_value(m_layoutGeneration));
        }
    }

    void DataGrid::SortBy(int column)
    {
        if (!m_pController)
            return;
        // Same column again toggles the direction
        SetSort(column, m_pController->GetLastSortColumn() == column ? !m_pController->IsLastSortAscending() : true);
    }

    void DataGrid::SetSort(int column, bool bAscending)
    {
        if (!m_pController || column < 0 || column >= static_cast<int>(m_pController->GetColumns().size()))
            return;
        m_pController->Sort(column, bAscending);
        UpdateSortIndicators();
        Refresh();
        SaveLayout();
        if (m_onHeaderChanged)
            m_onHeaderChanged();
    }

    double DataGrid::VerticalOffset()
    {
        auto scroller = Scroller();
        return scroller ? scroller.VerticalOffset() : 0.0;
    }

    void DataGrid::ScrollTo(double offset)
    {
        if (auto scroller = Scroller())
        {
            scroller.ChangeView(nullptr, offset, nullptr, true);
            m_list.UpdateLayout();
        }
    }

    std::vector<winrt::weak_ref<Grid>> DataGrid::HeaderCellRefs() const
    {
        std::vector<winrt::weak_ref<Grid>> refs;
        for (auto const &child : m_header.Children())
        {
            if (auto cell = child.try_as<Grid>())
                refs.push_back(winrt::make_weak(cell));
        }
        return refs;
    }

    // --- Rows --------------------------------------------------------------------------------

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
            // selection. A changed order with about the same row count is written into the existing
            // rows. Anything else (first population, filter change, many rows more or fewer)
            // becomes one ReplaceAll: one Reset notification instead of thousands of events, with
            // selection and scroll position restored afterwards.
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
            else if (!m_current.empty() && (std::max(next.size(), m_current.size()) - std::min(next.size(), m_current.size())) <= maxIncrementalChanges)
            {
                // The order changed (a re-sort; an auto-refresh of a view sorted by a value that
                // changes) and the row count is about the same: the rows stay and show other
                // objects. No reset, so scroll position, keyboard focus and containers are kept;
                // the selection is moved to where its objects are now.
                const auto selection = GetSelection();
                while (m_items.Size() > next.size())
                    m_items.RemoveAtEnd();
                const uint32_t common = m_items.Size();
                for (uint32_t i = 0; i < common; ++i)
                {
                    if (m_current[i] != next[i])
                        get_self<RowItem>(m_items.GetAt(i).as<winrt::Windows::Foundation::IStringable>())->Set(next[i]);
                }
                for (size_t i = common; i < next.size(); ++i)
                    m_items.Append(make<RowItem>(next[i]));
                m_current = std::move(next);
                if (!selection.empty() && GetSelection() != selection)
                {
                    m_list.DeselectRange(winrt::Microsoft::UI::Xaml::Data::ItemIndexRange{0, static_cast<uint32_t>(m_current.size())});
                    RestoreSelection({selection.begin(), selection.end()});
                }
            }
            else
            {
                const auto selection = GetSelection();
                const std::unordered_set<pserv::DataObject *> selected(selection.begin(), selection.end());
                auto scroller = Scroller();
                const double offset = scroller ? scroller.VerticalOffset() : 0.0;

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

                RestoreSelection(selected);
                if (scroller) // also at 0: the ListView moves the offset after a reset (executed: 0 -> 6209)
                {
                    m_list.UpdateLayout();
                    scroller.ChangeView(nullptr, offset, nullptr, true);
                    // Applied now: the ListView adjusts the offset after a reset on its own,
                    // and a lazily applied ChangeView lost against that (executed: 240 -> 264).
                    m_list.UpdateLayout();
                }
            }
        }
        UpdateRealized();
        m_stats.refreshMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        ++m_stats.refreshes;
    }

    void DataGrid::RestoreSelection(const std::unordered_set<pserv::DataObject *> &selected)
    {
        if (selected.empty())
            return;
        // Contiguous runs as one range each
        int first = -1;
        for (int i = 0; i <= static_cast<int>(m_current.size()); ++i)
        {
            const bool bSelected = i < static_cast<int>(m_current.size()) && selected.contains(m_current[static_cast<size_t>(i)]);
            if (bSelected && first < 0)
                first = i;
            else if (!bSelected && first >= 0)
            {
                m_list.SelectRange(winrt::Microsoft::UI::Xaml::Data::ItemIndexRange{first, static_cast<uint32_t>(i - first)});
                first = -1;
            }
        }
    }

    void DataGrid::BuildRowCells(Grid const &row)
    {
        row.Children().Clear();
        row.ColumnDefinitions().Clear();
        const auto &columns = m_pController->GetColumns();
        for (size_t d = 0; d < m_order.size(); ++d)
        {
            const auto c = static_cast<size_t>(m_order[d]);
            ColumnDefinition cd;
            cd.Width(GridLengthHelper::FromPixels(m_widths[c] * m_zoom));
            row.ColumnDefinitions().Append(cd);
            TextBlock cell;
            cell.VerticalAlignment(VerticalAlignment::Center);
            cell.FontSize(FontSize());
            if (m_fontFamily)
                cell.FontFamily(m_fontFamily);
            cell.TextTrimming(TextTrimming::CharacterEllipsis);
            cell.Padding(ThicknessHelper::FromLengths(8, 0, 8, 0));
            cell.TextAlignment(columns[c].GetAlignment() == pserv::ColumnAlignment::Right ? TextAlignment::Right : TextAlignment::Left);
            Grid::SetColumn(cell, static_cast<int>(d));
            row.Children().Append(cell);
        }
        row.Tag(box_value(m_layoutGeneration));
    }

    void DataGrid::OnContainerContentChanging(ContainerContentChangingEventArgs const &args)
    {
        if (args.InRecycleQueue() || !m_pController)
            return;
        const auto start = std::chrono::steady_clock::now();
        ApplyRowHeight(args.ItemContainer());
        auto row = args.ItemContainer().ContentTemplateRoot().as<Grid>();
        const auto stamp = row.Tag() ? unbox_value<int>(row.Tag()) : -1;
        if (stamp != m_layoutGeneration)
            BuildRowCells(row); // (re)build the cells for the current column layout
        FillRow(row, ObjectOf(args.Item()), args.ItemIndex());
        args.Handled(true);
        m_stats.containerMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        ++m_stats.containers;
    }

    void DataGrid::FillRow(Grid const &row, pserv::DataObject *pObject, int index)
    {
        // Every second row is tinted; the index of a realized row changes when rows come and go
        const Brush &background = index % 2 ? m_alternateBrush : m_noBrush;
        if (row.Background() != background)
            row.Background(background);
        const auto state = m_pController->GetVisualState(pObject);
        const Brush &brush = state == pserv::VisualState::Highlighted ? m_highlightBrush : (state == pserv::VisualState::Disabled ? m_disabledBrush : m_normalBrush);
        auto cells = row.Children();
        for (uint32_t d = 0; d < cells.Size() && d < m_order.size(); ++d)
        {
            auto cell = cells.GetAt(d).as<TextBlock>();
            hstring text = to_hstring(pObject->GetProperty(m_order[d]));
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
            ApplyRowHeight(container);
            auto row = container.ContentTemplateRoot().try_as<Grid>();
            if (!row || !row.Tag())
                continue;
            if (unbox_value<int>(row.Tag()) != m_layoutGeneration)
                BuildRowCells(row);
            FillRow(row, m_current[static_cast<size_t>(i)], i);
        }
    }

    DataGrid::StateCounts DataGrid::CountStates() const
    {
        StateCounts counts;
        if (!m_pController)
            return counts;
        for (auto *obj : m_current)
        {
            const auto state = m_pController->GetVisualState(obj);
            counts.highlighted += state == pserv::VisualState::Highlighted;
            counts.disabled += state == pserv::VisualState::Disabled;
        }
        return counts;
    }

    // --- Selection and copy -----------------------------------------------------------------

    std::vector<pserv::DataObject *> DataGrid::GetSelection() const
    {
        std::vector<int> indices;
        for (auto const &range : m_list.SelectedRanges())
        {
            for (int i = range.FirstIndex(); i <= range.LastIndex() && i < static_cast<int>(m_current.size()); ++i)
                indices.push_back(i);
        }
        std::sort(indices.begin(), indices.end());
        std::vector<pserv::DataObject *> selection;
        selection.reserve(indices.size());
        for (int i : indices)
            selection.push_back(m_current[static_cast<size_t>(i)]);
        return selection;
    }

    pserv::DataObject *DataGrid::ObjectFromContainer(DependencyObject const &element) const
    {
        for (DependencyObject e = element; e; e = VisualTreeHelper::GetParent(e))
        {
            if (auto item = e.try_as<ListViewItem>())
            {
                const auto content = m_list.ItemFromContainer(item);
                return content ? ObjectOf(content) : nullptr;
            }
            if (e == m_list)
                break;
        }
        return nullptr;
    }

    void DataGrid::SelectOnly(pserv::DataObject *pObject)
    {
        const auto it = std::find(m_current.begin(), m_current.end(), pObject);
        if (it == m_current.end())
            return;
        const int index = static_cast<int>(it - m_current.begin());
        if (!m_current.empty())
            m_list.DeselectRange(winrt::Microsoft::UI::Xaml::Data::ItemIndexRange{0, static_cast<uint32_t>(m_current.size())});
        m_list.SelectRange(winrt::Microsoft::UI::Xaml::Data::ItemIndexRange{index, 1});
    }

    std::string DataGrid::FormatSelection() const
    {
        const auto selection = GetSelection();
        if (!m_pController || selection.empty())
            return {};
        const auto &columns = m_pController->GetColumns();
        std::string text;
        for (size_t d = 0; d < m_order.size(); ++d)
            text += (d ? "\t" : "") + columns[static_cast<size_t>(m_order[d])].DisplayName;
        for (const auto *obj : selection)
        {
            text += "\r\n";
            for (size_t d = 0; d < m_order.size(); ++d)
                text += (d ? "\t" : "") + obj->GetProperty(m_order[d]);
        }
        return text;
    }
} // namespace pserv6
