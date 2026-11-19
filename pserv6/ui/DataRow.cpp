#include "pch.h"
#include "DataRow.h"
#include "DataRow.g.cpp"
#include <utils/string_utils.h>

namespace winrt::pserv6::implementation
{
    DataRow::~DataRow()
    {
        if (m_pObject) m_pObject->Release(REFCOUNT_DEBUG_ARGS);
    }
    void DataRow::Update(pserv::DataObject* object, std::vector<int> const& columns,
        std::vector<double> const& widths, std::vector<pserv::DataObjectColumn> const& metadata)
    {
        object->Retain(REFCOUNT_DEBUG_ARGS);
        if (m_pObject) m_pObject->Release(REFCOUNT_DEBUG_ARGS);
        m_pObject = object;
        while (m_cells.Size() > columns.size()) m_cells.RemoveAtEnd();
        for (uint32_t i = 0; i < columns.size(); ++i)
        {
            if (i == m_cells.Size()) m_cells.Append(make<implementation::DataCell>());
            auto cell = get_self<implementation::DataCell>(m_cells.GetAt(i));
            cell->Update(hstring(pserv::utils::Utf8ToWide(object->GetProperty(columns[i]))), widths[i],
                metadata[columns[i]].GetAlignment() == pserv::ColumnAlignment::Right ?
                Microsoft::UI::Xaml::TextAlignment::Right : Microsoft::UI::Xaml::TextAlignment::Left);
        }
    }
    void DataRow::SetForeground(Microsoft::UI::Xaml::Media::Brush const& brush)
    {
        for (auto const& cell : m_cells) get_self<implementation::DataCell>(cell)->SetForeground(brush);
    }
    void DataRow::SetWidth(uint32_t column, double width)
    {
        get_self<implementation::DataCell>(m_cells.GetAt(column))->SetWidth(width);
    }
}
