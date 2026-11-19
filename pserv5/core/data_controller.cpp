#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_object.h>
#include <core/data_action.h>
#include <core/exporters/exporter_registry.h>
#include <utils/string_utils.h>
#include <core/data_action_dispatch_context.h>

namespace pserv
{
    int DataController::FindColumnPropertyIdMismatch() const
    {
        for (size_t i = 0; i < m_columns.size(); ++i)
        {
            if (m_columns[i].PropertyId != static_cast<int>(i))
                return static_cast<int>(i);
        }
        return -1;
    }

    void DataController::LogColumnPropertyIdMismatches() const
    {
        for (size_t i = 0; i < m_columns.size(); ++i)
        {
            if (m_columns[i].PropertyId != static_cast<int>(i))
            {
                spdlog::error("Controller '{}': column {} ('{}') shows property {}; columns must be in property-id order",
                    m_controllerName, i, m_columns[i].DisplayName, m_columns[i].PropertyId);
            }
        }
    }

    DataController::~DataController()
    {
        Clear();
    }

    void DataController::Sort(int columnIndex, bool ascending)
    {
        if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size()))
            return;

        // Remember last sort for re-applying after refresh
        m_lastSortColumn = columnIndex;
        m_lastSortAscending = ascending;

        ColumnDataType dataType = m_columns[columnIndex].DataType;
        m_objects.Sort(columnIndex, ascending, dataType);
    }
    
    void DataController::Clear()
    {
        m_objects.Clear();
        m_bLoaded = false;
    }

} // namespace pserv
