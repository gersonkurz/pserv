#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_object.h>
#include <core/data_action.h>
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

        // Add filter option (global text search)
        cmd.add_argument("--filter")
            .help("Filter results by text (case-insensitive substring match across all fields)")
            .default_value(std::string(""));

        // Add sort option
        cmd.add_argument("--sort")
            .help("Sort by column name (ascending by default)")
            .default_value(std::string(""));

        // Add descending sort flag
        cmd.add_argument("--desc")
            .help("Sort in descending order (use with --sort)")
            .default_value(false)
            .implicit_value(true);

        // Add column-specific filter arguments dynamically
        for (const auto &col : m_columns)
        {
            std::string argName = "--col-" + utils::ToLower(col.BindingName);
            std::string helpText = "Filter by " + col.DisplayName + " (case-insensitive substring match)";
            spdlog::debug("RegisterArguments: Adding argument '{}' for column '{}' (BindingName='{}')",
                          argName, col.DisplayName, col.BindingName);
            cmd.add_argument(argName)
                .help(helpText)
                .default_value(std::string(""));
        }

        // Register action subcommands
        std::vector<const DataAction *> actions = GetAllActions();
        std::string actionsList;

        for (const DataAction *action : actions)
        {
            // Skip separators (not real actions)
            if (action->IsSeparator())
                continue;

            // Create action subcommand name (e.g., "services start", "services stop")
            std::string action_name = utils::ToLower(action->GetName());
            std::replace(action_name.begin(), action_name.end(), ' ', '-');

            // Add to actions list for help text
            if (!actionsList.empty())
                actionsList += ", ";
            actionsList += action_name;

            spdlog::debug("RegisterArguments: Adding action subcommand '{}'", action_name);

            // Create action subparser
            subparsers.push_back(std::make_unique<argparse::ArgumentParser>(
                action_name, "5.0.0", argparse::default_arguments::help, false));
            auto &action_cmd = *subparsers.back();
            action_cmd.add_description(action->GetName());

            // Add positional argument(s) for target name(s)
            action_cmd.add_argument("targets")
                .help("Target object(s) to act upon (by " + m_columns[0].DisplayName + ")")
                .remaining();

            // Add --force flag for destructive actions
            if (action->IsDestructive())
            {
                action_cmd.add_argument("--force")
                    .help("Skip confirmation prompt")
                    .default_value(false)
                    .implicit_value(true);
            }

            // Let action register custom arguments
            action->RegisterArguments(action_cmd);

            // Add action subcommand to controller subcommand
            cmd.add_subparser(action_cmd);
        }

        // Add epilog listing available actions
        if (!actionsList.empty())
        {
            cmd.add_epilog("Available actions: " + actionsList + "\nUse 'pservc " + cmd_name + " <action> --help' for action-specific help");
        }

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
