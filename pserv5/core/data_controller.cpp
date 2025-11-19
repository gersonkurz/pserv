#include "precomp.h"
#include "data_controller.h"
#include <algorithm>

namespace pserv {

void DataController::Sort(int columnIndex, bool ascending) {
    if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size())) return;

    // Remember last sort for re-applying after refresh
    m_lastSortColumn = columnIndex;
    m_lastSortAscending = ascending;

    auto& objects = const_cast<std::vector<DataObject*>&>(GetDataObjects());
    ColumnDataType dataType = m_columns[columnIndex].DataType;

    std::sort(objects.begin(), objects.end(),
        [columnIndex, ascending, dataType](const DataObject* a, const DataObject* b) {
        PropertyValue valA = a->GetTypedProperty(columnIndex);
        PropertyValue valB = b->GetTypedProperty(columnIndex);

        // Numeric comparison for Integer, UnsignedInteger, and Size types
        if (dataType == ColumnDataType::Integer ||
            dataType == ColumnDataType::UnsignedInteger ||
            dataType == ColumnDataType::Size) {

            uint64_t numA = 0, numB = 0;

            if (std::holds_alternative<uint64_t>(valA)) {
                numA = std::get<uint64_t>(valA);
            } else if (std::holds_alternative<int64_t>(valA)) {
                numA = static_cast<uint64_t>(std::get<int64_t>(valA));
            }

            if (std::holds_alternative<uint64_t>(valB)) {
                numB = std::get<uint64_t>(valB);
            } else if (std::holds_alternative<int64_t>(valB)) {
                numB = static_cast<uint64_t>(std::get<int64_t>(valB));
            }

            return ascending ? (numA < numB) : (numA > numB);
        }

        // String comparison for String and Time types
        std::string strA, strB;

        if (std::holds_alternative<std::string>(valA)) {
            strA = std::get<std::string>(valA);
        } else {
            strA = a->GetProperty(columnIndex);
        }

        if (std::holds_alternative<std::string>(valB)) {
            strB = std::get<std::string>(valB);
        } else {
            strB = b->GetProperty(columnIndex);
        }

        int cmp = strA.compare(strB);
        return ascending ? (cmp < 0) : (cmp > 0);
    });
}

} // namespace pserv
