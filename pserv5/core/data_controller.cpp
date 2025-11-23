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
#ifndef PSERV_CONSOLE_BUILD
        if (m_pPropertiesDialog)
        {
            m_pPropertiesDialog->Close();
            m_pPropertiesDialog = nullptr;
        }
#endif
        Clear();
    }

#ifndef PSERV_CONSOLE_BUILD
    void DataController::ShowPropertiesDialog(DataActionDispatchContext &ctx)
    {
        if (!ctx.m_selectedObjects.empty())
        {
            m_pPropertiesDialog = DBG_NEW DataPropertiesDialog{this, ctx.m_selectedObjects, ctx.m_hWnd};
            m_pPropertiesDialog->Open();
        }
    }
#endif
#ifdef PSERV_CONSOLE_BUILD
    void DataController::RegisterArguments(argparse::ArgumentParser &program, std::vector<std::unique_ptr<argparse::ArgumentParser>> &subparsers) const
    {
        // Create a subcommand for this controller
        std::string cmd_name(GetControllerName());
        // Convert to lowercase and replace spaces with hyphens for command names
        std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::tolower);
        std::replace(cmd_name.begin(), cmd_name.end(), ' ', '-');

        // Create subparser for this controller and store in persistent vector
        // argparse stores references, so the ArgumentParser must outlive the main program parser
        // Using unique_ptr because ArgumentParser has deleted copy/move constructors
        // Disable exit_on_default_arguments to prevent std::exit() on --help, allowing proper cleanup
        subparsers.push_back(std::make_unique<argparse::ArgumentParser>(
            cmd_name, "5.0.0", argparse::default_arguments::help, false));
        auto &cmd = *subparsers.back();
        cmd.add_description("Manage " + std::string(GetItemName()) + "s");

        // Add common output format option
        cmd.add_argument("--format")
            .help("Output format: table, json, csv")
            .default_value(std::string("table"));

        // Add filter option
        cmd.add_argument("--filter")
            .help("Filter results by text (case-insensitive substring match)")
            .default_value(std::string(""));

        // Add sort option
        cmd.add_argument("--sort")
            .help("Sort by column name")
            .default_value(std::string(""));

        // Add the subparser to the main program
        program.add_subparser(cmd);
    }
#endif

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
    
#ifndef PSERV_CONSOLE_BUILD

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
#endif

    void DataController::Clear()
    {
        m_objects.Clear();
        m_bLoaded = false;
    }

} // namespace pserv
