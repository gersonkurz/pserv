#pragma once
// The pserv6 grid (plan D6): a virtualized ListView whose columns are generated from the current
// controller's GetColumns(). Rows are filled on demand (ContainerContentChanging); a refresh is
// applied as an identity diff on the item collection plus an in-place update of the realized rows.
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
        void Attach(winrt::Microsoft::UI::Xaml::Controls::ListView const &list, winrt::Microsoft::UI::Xaml::Controls::Grid const &header);

        /// Show another controller: rebuild header and rows from its current live objects.
        void SetController(pserv::DataController *pController);

        /// Re-apply the controller's live objects (after a publish, a sort or a filter change).
        void Refresh();

        void SetFilter(std::string lowerCaseFilter);

        /// Called from the ListView's ContainerContentChanging handler.
        void OnContainerContentChanging(winrt::Microsoft::UI::Xaml::Controls::ContainerContentChangingEventArgs const &args);

        /// Number of rows currently shown (after filtering).
        size_t RowCount() const { return m_current.size(); }

        /// The selected objects (borrowed pointers, valid until the next publish).
        std::vector<pserv::DataObject *> GetSelection() const;

        /// Invoked after a header click changed the sort (the owner may persist it).
        std::function<void()> m_onSortChanged;

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
        void BuildHeader();
        void FillRow(winrt::Microsoft::UI::Xaml::Controls::Grid const &row, pserv::DataObject *pObject);
        void UpdateRealized();
        void SortBy(int column);

        winrt::Microsoft::UI::Xaml::Controls::ListView m_list{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::Grid m_header{nullptr};
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Windows::Foundation::IInspectable> m_items{nullptr};
        std::vector<pserv::DataObject *> m_current; ///< Parallel to m_items (each RowItem holds a reference).
        pserv::DataController *m_pController{nullptr};
        std::vector<double> m_widths;
        std::string m_filter;
        int m_layoutGeneration{0}; ///< Bumped when the column layout changes; stamped on row containers.
        winrt::Microsoft::UI::Xaml::Media::Brush m_normalBrush{nullptr}, m_highlightBrush{nullptr}, m_disabledBrush{nullptr};
    };
} // namespace pserv6
