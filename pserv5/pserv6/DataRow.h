#pragma once
#include "DataRow.g.h"
#include "DataCell.h"
#include <core/data_object.h>
#include <core/data_object_column.h>

namespace winrt::pserv6::implementation
{
    // Stable UI identity, with independently retained, immutable model snapshots.
    struct DataRow : DataRowT<DataRow>
    {
        DataRow() = default;
        ~DataRow();
        Windows::Foundation::Collections::IVector<pserv6::DataCell> Cells() const { return m_cells; }
        void Update(pserv::DataObject* object, std::vector<int> const& columns,
            std::vector<double> const& widths, std::vector<pserv::DataObjectColumn> const& metadata);
        void SetForeground(Microsoft::UI::Xaml::Media::Brush const& brush);
        void SetWidth(uint32_t column, double width);
        pserv::DataObject* Object() const { return m_pObject; }
    private:
        pserv::DataObject* m_pObject{nullptr};
        // XAML ItemsSource queries IVector<IInspectable>, not IVector<DataCell>.
        // The observable factory exposes both interfaces; the plain typed vector does not.
        Windows::Foundation::Collections::IVector<pserv6::DataCell> m_cells{single_threaded_observable_vector<pserv6::DataCell>()};
    };
}
namespace winrt::pserv6::factory_implementation
{
    struct DataRow : DataRowT<DataRow, implementation::DataRow> {};
}
