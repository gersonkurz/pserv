#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_object.h>
#include <core/exporters/exporter_registry.h>
#include <dialogs/data_properties_dialog.h>
#include <utils/file_dialogs.h>
#include <utils/string_utils.h>
#include <core/data_action_dispatch_context.h>

namespace pserv
{
    DataController::~DataController()
    {
        if (m_pPropertiesDialog)
        {
            m_pPropertiesDialog->Close();
            m_pPropertiesDialog = nullptr;
        }
        Clear();
    }

    void DataController::ShowPropertiesDialog(DataActionDispatchContext &ctx)
    {
        if (!ctx.m_selectedObjects.empty())
        {
            m_pPropertiesDialog = new DataPropertiesDialog{this, ctx.m_selectedObjects};
            m_pPropertiesDialog->Open();
        }
    }

    void DataController::Sort(int columnIndex, bool ascending)
    {
        if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size()))
            return;

        // Remember last sort for re-applying after refresh
        m_lastSortColumn = columnIndex;
        m_lastSortAscending = ascending;

        auto &objects = const_cast<std::vector<DataObject *> &>(GetDataObjects());
        ColumnDataType dataType = m_columns[columnIndex].DataType;

        std::sort(objects.begin(),
            objects.end(),
            [columnIndex, ascending, dataType](const DataObject *a, const DataObject *b)
            {
                PropertyValue valA = a->GetTypedProperty(columnIndex);
                PropertyValue valB = b->GetTypedProperty(columnIndex);

                // Numeric comparison for Integer, UnsignedInteger, and Size types
                if (dataType == ColumnDataType::Integer || dataType == ColumnDataType::UnsignedInteger || dataType == ColumnDataType::Size)
                {

                    uint64_t numA = 0, numB = 0;

                    if (std::holds_alternative<uint64_t>(valA))
                    {
                        numA = std::get<uint64_t>(valA);
                    }
                    else if (std::holds_alternative<int64_t>(valA))
                    {
                        numA = static_cast<uint64_t>(std::get<int64_t>(valA));
                    }

                    if (std::holds_alternative<uint64_t>(valB))
                    {
                        numB = std::get<uint64_t>(valB);
                    }
                    else if (std::holds_alternative<int64_t>(valB))
                    {
                        numB = static_cast<uint64_t>(std::get<int64_t>(valB));
                    }

                    return ascending ? (numA < numB) : (numA > numB);
                }

                // String comparison for String and Time types
                std::string strA, strB;

                if (std::holds_alternative<std::string>(valA))
                {
                    strA = std::get<std::string>(valA);
                }
                else
                {
                    strA = a->GetProperty(columnIndex);
                }

                if (std::holds_alternative<std::string>(valB))
                {
                    strB = std::get<std::string>(valB);
                }
                else
                {
                    strB = b->GetProperty(columnIndex);
                }

                int cmp = strA.compare(strB);
                return ascending ? (cmp < 0) : (cmp > 0);
            });
    }

    void DataController::RenderPropertiesDialog()
    {
        // Render properties dialog if open
        if (m_pPropertiesDialog)
        {
            const bool changesApplied = m_pPropertiesDialog->Render();
            if (changesApplied)
            {
                // Refresh to show updated data
                Refresh();
            }

            // Check if dialog was closed and clean up
            if (!m_pPropertiesDialog->IsOpen())
            {
                spdlog::info("DataController::RenderPropertiesDialog - dialog closed, cleaning up");
                delete m_pPropertiesDialog;
                m_pPropertiesDialog = nullptr;
            }
        }
    }
    void DataController::Clear()
    {
        for (auto *mod : m_objects)
        {
            delete mod;
        }
        m_objects.clear();
        m_bLoaded = false;
    }

} // namespace pserv
