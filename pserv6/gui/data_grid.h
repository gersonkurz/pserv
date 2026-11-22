#pragma once
// The pserv6 grid (plan D6): a virtualized ListView whose columns are generated from the current
// controller's GetColumns(). Rows are filled on demand (ContainerContentChanging); a refresh is
// applied as an identity diff on the item collection plus an in-place update of the realized rows.
//
// Column layout per view (widths, display order, sort) is kept in the view's DisplayTable settings,
// in pserv5's format so imported settings apply: widths and display positions are listed by
// column index.
#include <core/data_controller.h>

namespace pserv6
{
    class DataGrid final
    {
    public:
        DataGrid() = default;
        DataGrid(const DataGrid &) = delete;
        DataGrid &operator=(const DataGrid &) = delete;

        /// Bind to the XAML elements (once, after InitializeComponent).
        void Attach(winrt::Microsoft::UI::Xaml::Controls::ListView const &list, winrt::Microsoft::UI::Xaml::Controls::Grid const &header,
            winrt::Microsoft::UI::Xaml::Controls::ScrollViewer const &headerScroller);

        /// The brushes the grid paints with. They are theme resources resolved by the window (for
        /// its current theme) and set again whenever the theme changes.
        struct Brushes
        {
            winrt::Microsoft::UI::Xaml::Media::Brush normal{nullptr}, highlight{nullptr}, disabled{nullptr}, gripper{nullptr}, hover{nullptr}, alternate{nullptr};
        };
        void SetBrushes(const Brushes &brushes);

        /// Zoom factor for the rows and the header (1.0 = 14 px text in 24 px rows).
        /// Column widths are stored unzoomed and shown multiplied by it.
        void SetZoom(double zoom);
        double Zoom() const { return m_zoom; }

        /// Font family of the rows and the header; empty for the default (Segoe UI Variable).
        void SetFontFamily(const std::wstring &name);

        /// Show another controller: restore its column layout and sort, rebuild header and rows.
        void SetController(pserv::DataController *pController);

        /// Re-apply the controller's live objects (after a publish, a sort or a filter change).
        void Refresh();

        void SetFilter(std::string lowerCaseFilter);

        /// Called from the ListView's ContainerContentChanging handler.
        void OnContainerContentChanging(winrt::Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const &args);

        /// Number of rows currently shown (after filtering).
        size_t RowCount() const { return m_current.size(); }

        /// How many of the shown rows are highlighted and disabled (status bar).
        struct StateCounts
        {
            size_t highlighted{0}, disabled{0};
        };
        StateCounts CountStates() const;

        /// The selected objects in display order (borrowed pointers, valid until the next publish).
        std::vector<pserv::DataObject *> GetSelection() const;

        /// The object shown in the given row container, or nullptr.
        pserv::DataObject *ObjectFromContainer(winrt::Microsoft::UI::Xaml::DependencyObject const &element) const;

        /// Make `pObject` the only selected row (used before a context menu on an unselected row).
        void SelectOnly(pserv::DataObject *pObject);

        /// Tab-separated text of the selected rows (header line first), columns in display order.
        std::string FormatSelection() const;

        /// Sort by a column, as a header click does (same column again toggles the direction).
        void SortBy(int column);

        /// Sort by a column in a given direction (the keyboard path: the toolbar's Sort menu).
        void SetSort(int column, bool bAscending);

        /// Invoked when the sort, the column order or the controller changed (to update menus).
        std::function<void()> m_onHeaderChanged;

        /// Vertical scroll position of the rows (self-test).
        double VerticalOffset();
        void ScrollTo(double offset);

        /// Weak references to the current header cells (the self-test checks that rebuilt
        /// headers are released, i.e. that no handler keeps its element alive).
        std::vector<winrt::weak_ref<winrt::Microsoft::UI::Xaml::Controls::Grid>> HeaderCellRefs() const;

        /// Column layout, display position -> column index (for tests and the self-test).
        const std::vector<int> &DisplayOrder() const { return m_fullOrder; }

        /// Show or hide a column (by column index); the last shown column cannot be hidden.
        bool IsColumnHidden(int column) const { return m_hidden[static_cast<size_t>(column)]; }
        void SetColumnHidden(int column, bool bHidden);
        size_t ShownColumnCount() const { return m_order.size(); }

        /// Back to the default widths and order, every column shown.
        void ResetLayout();
        const std::vector<double> &Widths() const { return m_widths; }

        /// Move the column at display position `from` to display position `to`.
        void MoveColumn(int from, int to);

        /// Set the width of a column (by column index), live, and persist it.
        void SetColumnWidth(int column, double width);

        /// Fit a column (by column index) to its header and its content (double-click on the
        /// column's right edge).
        void AutoSizeColumn(int column);

        /// UI-thread cost counters, read and reset by the self-test.
        struct Stats
        {
            double refreshMs{0};
            int refreshes{0};
            int resets{0};
            double containerMs{0};
            int containers{0};
        } m_stats;

    private:
        void LoadLayout();
        void SaveLayout() const;
        void RebuildShown();  ///< m_order from m_fullOrder and m_hidden
        void LayoutChanged(); ///< After a change of order or visibility: rebuild, save, notify
        void BuildHeader();
        void UpdateSortIndicators();
        void ApplyWidthsToRealizedRows();
        void BuildRowCells(winrt::Microsoft::UI::Xaml::Controls::Grid const &row);
        void FillRow(winrt::Microsoft::UI::Xaml::Controls::Grid const &row, pserv::DataObject *pObject, int index);
        void UpdateRealized();
        double RowHeight() const { return std::round(24.0 * m_zoom); }
        double FontSize() const { return 14.0 * m_zoom; }
        void ApplyRowHeight(winrt::Microsoft::UI::Xaml::Controls::Primitives::SelectorItem const &container) const;
        void RestoreSelection(const std::unordered_set<pserv::DataObject *> &selected);
        int DisplayPositionAt(double x) const;
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewer Scroller();

        winrt::Microsoft::UI::Xaml::Controls::ListView m_list{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::Grid m_header{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewer m_scroller{nullptr};       ///< The list's own (both directions)
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewer m_headerScroller{nullptr}; ///< Around the header, kept at the list's horizontal offset
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_items{nullptr};
        std::vector<pserv::DataObject *> m_current; ///< Parallel to m_items (each RowItem holds a reference).
        pserv::DataController *m_pController{nullptr};
        std::vector<double> m_widths; ///< By column index.
        std::vector<int> m_fullOrder; ///< Display position -> column index, hidden columns included (persisted).
        std::vector<bool> m_hidden;   ///< By column index.
        std::vector<int> m_order;     ///< Display position -> column index of the shown columns.
        std::vector<winrt::Microsoft::UI::Xaml::Controls::TextBlock> m_sortGlyphs; ///< By display position.
        std::string m_filter;
        double m_zoom{1.0};
        winrt::Microsoft::UI::Xaml::Media::FontFamily m_fontFamily{nullptr}; ///< Null: the default font
        int m_layoutGeneration{0}; ///< Bumped when the column layout changes; stamped on row containers.

        // Header interaction: click a cell to sort, drag a cell to reorder, drag a gripper to resize
        struct HeaderDrag
        {
            int fromPosition{-1};
            double startX{0};
            bool bMoved{false};
        } m_headerDrag;
        struct Resize
        {
            int column{-1};
            double startX{0};
            double startWidth{0};
        } m_resize;

        winrt::Microsoft::UI::Xaml::Media::Brush m_normalBrush{nullptr}, m_highlightBrush{nullptr}, m_disabledBrush{nullptr};
        winrt::Microsoft::UI::Xaml::Media::Brush m_gripperBrush{nullptr}, m_dropBrush{nullptr}, m_alternateBrush{nullptr}, m_noBrush{nullptr};
    };
} // namespace pserv6
