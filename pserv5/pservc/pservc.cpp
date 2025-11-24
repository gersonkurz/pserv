#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_controller_library.h>
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <argparse/argparse.hpp>
#include <pservc/console.h>
#include <pservc/console_table.h>
#include <utils/logging.h>
#include <utils/base_app.h>
#include <utils/string_utils.h>
#include <core/async_operation.h>

using namespace pserv;

int main(int argc, char *argv[])
{
    utils::BaseApp baseApp;

    console::write_line(CONSOLE_FOREGROUND_GREEN "*** pservc " PSERV_VERSION_STRING " ***" CONSOLE_STANDARD);
    DataControllerLibrary dataControllerLibrary;
    // Disable exit_on_default_arguments to prevent std::exit() on --help/--version
    // This allows destructors to run properly and prevents memory leak reports
    argparse::ArgumentParser program{"pservc", PSERV_VERSION_STRING, argparse::default_arguments::all, false};

    // Storage for subparsers - must outlive the program ArgumentParser
    // argparse stores references, so these must remain alive
    // Using unique_ptr because ArgumentParser has deleted copy/move constructors
    std::vector<std::unique_ptr<argparse::ArgumentParser>> subparsers;

    for (const auto controller : dataControllerLibrary.GetDataControllers())
    {
        controller->RegisterArguments(program, subparsers);
    }
    try
    {
        program.parse_args(argc, argv);
    }
    catch (const std::exception &err)
    {
        console::write_line(CONSOLE_FOREGROUND_RED + std::string(err.what()) + CONSOLE_STANDARD);
        console::write(program.help().str());
        return 1;
    }

    // Note: argparse handles --help and --version automatically
    // We disabled exit_on_default_arguments to allow cleanup, but argparse still prints help/version
    // So we just need to check if they were requested and exit gracefully

    // Find which subcommand was used
    DataController *selectedController = nullptr;
    argparse::ArgumentParser *selectedSubparser = nullptr;
    size_t subparserIndex = 0;

    for (const auto controller : dataControllerLibrary.GetDataControllers())
    {
        // Convert controller name to lowercase-hyphenated format
        std::string cmd_name(controller->GetControllerName());
        std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::tolower);
        std::replace(cmd_name.begin(), cmd_name.end(), ' ', '-');

        if (program.is_subcommand_used(cmd_name))
        {
            selectedController = controller;
            selectedSubparser = subparsers[subparserIndex].get();
            break;
        }
        subparserIndex++;
    }

    // If no subcommand was provided, argparse will have printed help already
    if (!selectedController || !selectedSubparser)
    {
        return 0;
    }

    // Check if --help was requested by examining argv directly
    // (can't query argparse because --help is a default argument that's consumed during parsing)
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
        {
            // Argparse already printed help, just exit
            return 0;
        }
    }

    // Check if an action subcommand was used
    std::vector<const DataAction *> allActions = selectedController->GetAllActions();
    const DataAction *selectedAction = nullptr;
    argparse::ArgumentParser *selectedActionParser = nullptr;

    for (const DataAction *action : allActions)
    {
        if (action->IsSeparator())
            continue;

        std::string action_name = utils::ToLower(action->GetName());
        std::replace(action_name.begin(), action_name.end(), ' ', '-');

        // Check if this action subcommand was used
        // Wrap in try-catch because is_subcommand_used() throws std::out_of_range if subcommand doesn't exist
        try
        {
            if (selectedSubparser->is_subcommand_used(action_name))
            {
                selectedAction = action;
                // Get the action parser from the subparser
                try
                {
                    selectedActionParser = &selectedSubparser->at<argparse::ArgumentParser>(action_name);
                }
                catch (const std::exception &)
                {
                    // Action subparser not found
                }
                break;
            }
        }
        catch (const std::out_of_range &)
        {
            // Subcommand not registered - skip it
            continue;
        }
    }

    // If action was selected, dispatch to action execution
    if (selectedAction && selectedActionParser)
    {
        // Argparse will have printed help for action if --help was used

        // Load data
        console::write_line("Loading data...");
        selectedController->Refresh(false);

        // Get target names from positional arguments
        std::vector<std::string> targetNames;
        try
        {
            targetNames = selectedActionParser->get<std::vector<std::string>>("targets");
        }
        catch (const std::exception &)
        {
            // No targets provided
        }

        if (targetNames.empty())
        {
            console::write_line(CONSOLE_FOREGROUND_RED "Error: No target objects specified" CONSOLE_STANDARD);
            return 1;
        }

        // Find matching objects by first column (Name) - exact match, case-insensitive
        std::vector<DataObject *> selectedObjects;
        const auto &allObjects = selectedController->GetDataObjects();
        for (const std::string &targetName : targetNames)
        {
            std::string lowerTargetName = utils::ToLower(targetName);
            bool found = false;

            for (DataObject *obj : allObjects)
            {
                std::string objName = obj->GetProperty(0); // First column
                if (utils::ToLower(objName) == lowerTargetName)
                {
                    selectedObjects.push_back(obj);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                console::write_line(CONSOLE_FOREGROUND_RED "Error: Target '" + targetName + "' not found" CONSOLE_STANDARD);
                return 1;
            }
        }

        // Check --force for destructive actions
        if (selectedAction->IsDestructive())
        {
            bool force = false;
            try
            {
                force = selectedActionParser->get<bool>("--force");
            }
            catch (const std::exception &)
            {
            }

            if (!force)
            {
                console::write_line(CONSOLE_FOREGROUND_RED "Error: This is a destructive action. Use --force to confirm" CONSOLE_STANDARD);
                return 1;
            }
        }

        // Create dispatch context
        DataActionDispatchContext ctx;
        ctx.m_selectedObjects = selectedObjects;
        ctx.m_pController = selectedController;
        ctx.m_pActionParser = selectedActionParser;
        ctx.m_hWnd = nullptr;
        ctx.m_pAsyncOp = nullptr;

        // Execute action
        std::string actionName = selectedAction->GetName();
        size_t targetCount = selectedObjects.size();
        console::write_line(std::format("Executing action '{}' on {} target(s)...", actionName, targetCount));
        try
        {
            selectedAction->Execute(ctx);

            // If action created an async operation, wait for it
            if (ctx.m_pAsyncOp)
            {
                console::write_line("Working...");
                ctx.m_pAsyncOp->Wait();

                // Clean up async operation
                delete ctx.m_pAsyncOp;
                ctx.m_pAsyncOp = nullptr;
            }

            // If action needs refresh, do it
            if (ctx.m_bNeedsRefresh)
            {
                selectedController->Refresh(false);
            }

            console::write_line(CONSOLE_FOREGROUND_GREEN "Action completed successfully" CONSOLE_STANDARD);
            return 0;
        }
        catch (const std::exception &err)
        {
            console::write_line(CONSOLE_FOREGROUND_RED "Error executing action: " + std::string(err.what()) + CONSOLE_STANDARD);
            return 1;
        }
    }

    // No action selected - render list
    // Get the format argument from the subcommand parser (default to table)
    console::OutputFormat format = console::OutputFormat::Table;
    try
    {
        std::string formatStr = selectedSubparser->get<std::string>("--format");
        if (formatStr == "json")
        {
            format = console::OutputFormat::Json;
        }
        else if (formatStr == "csv")
        {
            format = console::OutputFormat::Csv;
        }
        else if (formatStr != "table")
        {
            console::write_line(CONSOLE_FOREGROUND_YELLOW "Warning: Unknown format '" + formatStr + "', using table format" CONSOLE_STANDARD);
        }
    }
    catch (const std::exception &)
    {
        // Format argument not provided, use default (table)
    }

    try
    {
        // Refresh the controller to load data
        console::write_line("Loading data...");
        selectedController->Refresh(false);

        // Get filter argument (if provided)
        std::string filter;
        try
        {
            filter = selectedSubparser->get<std::string>("--filter");
        }
        catch (const std::exception &)
        {
            // Filter not provided, use empty string (no filtering)
        }

        // Get sort argument (if provided)
        std::string sortColumn;
        try
        {
            sortColumn = selectedSubparser->get<std::string>("--sort");
        }
        catch (const std::exception &)
        {
            // Sort not provided, use empty string (will default to first column)
        }

        // Get descending flag (if provided)
        bool sortDescending = false;
        try
        {
            sortDescending = selectedSubparser->get<bool>("--desc");
        }
        catch (const std::exception &)
        {
            // Desc not provided, use ascending
        }

        // Apply sorting
        int sortColumnIndex = 0;
        bool sortAscending = !sortDescending;

        if (!sortColumn.empty())
        {
            // Find column by name (case-insensitive)
            const auto &columns = selectedController->GetColumns();
            bool found = false;

            for (size_t i = 0; i < columns.size(); ++i)
            {
                if (utils::ToLower(columns[i].DisplayName) == utils::ToLower(sortColumn) ||
                    utils::ToLower(columns[i].BindingName) == utils::ToLower(sortColumn))
                {
                    sortColumnIndex = static_cast<int>(i);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                console::write_line(CONSOLE_FOREGROUND_YELLOW "Warning: Column '" + sortColumn + "' not found, using first column" CONSOLE_STANDARD);
            }
        }

        selectedController->Sort(sortColumnIndex, sortAscending);

        // Parse column-specific filters
        std::map<int, std::string> columnFilters;
        const auto &columns = selectedController->GetColumns();
        for (size_t i = 0; i < columns.size(); ++i)
        {
            std::string argName = "--col-" + utils::ToLower(columns[i].BindingName);
            spdlog::debug("Parsing column filter: argName='{}', column='{}' (index={})",
                          argName, columns[i].DisplayName, i);
            try
            {
                std::string filterValue = selectedSubparser->get<std::string>(argName);
                spdlog::debug("  Found filter value: '{}'", filterValue);
                if (!filterValue.empty())
                {
                    columnFilters[static_cast<int>(i)] = filterValue;
                    spdlog::info("Column filter applied: column={} ({}), filter='{}'",
                                 i, columns[i].DisplayName, filterValue);
                }
            }
            catch (const std::exception &e)
            {
                // Column filter not provided
                spdlog::debug("  No filter provided: {}", e.what());
            }
        }

        // Render the data
        console::ConsoleTable table(selectedController, format);
        table.Render(selectedController->GetDataObjects(), filter, columnFilters);
    }
    catch (const std::exception &err)
    {
        console::write_line(CONSOLE_FOREGROUND_RED "Error: " + std::string(err.what()) + CONSOLE_STANDARD);
        return 1;
    }

    return 0;
}
