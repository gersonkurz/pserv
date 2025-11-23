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
            m_pPropertiesDialog = DBG_NEW DataPropertiesDialog{this, ctx.m_selectedObjects};
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

        ColumnDataType dataType = m_columns[columnIndex].DataType;
        m_objects.Sort(columnIndex, ascending, dataType);
    }
    
    bool DataController::HasPropertiesDialogWithEdits() const
    {
        return m_pPropertiesDialog && m_pPropertiesDialog->HasPendingEdits();
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
        m_objects.Clear();
        m_bLoaded = false;
    }

} // namespace pserv
