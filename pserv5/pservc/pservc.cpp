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

namespace
{
    /// @brief "Network Connections" -> "network-connections" (controller and action subcommand names).
    std::string ToCommandName(std::string_view name)
    {
        std::string cmd = utils::ToLower(std::string{name});
        std::replace(cmd.begin(), cmd.end(), ' ', '-');
        return cmd;
    }

    /// @brief The console's IActionHost: messages go to the console; there is no properties UI
    /// (theDataPropertiesAction is never registered as a CLI subcommand).
    class ConsoleActionHost final : public IActionHost
    {
    public:
        void ShowMessage(ActionMessageKind kind, std::string_view title, std::string_view text) override
        {
            const char *color = kind == ActionMessageKind::Error     ? CONSOLE_FOREGROUND_RED
                                : kind == ActionMessageKind::Warning ? CONSOLE_FOREGROUND_YELLOW
                                                                     : CONSOLE_STANDARD;
            console::write_line(std::string{color} + std::string{title} + ": " + std::string{text} + CONSOLE_STANDARD);
        }

        void ShowProperties(DataActionDispatchContext &) override {}
    };

    /// @brief Register one controller's subcommand, its options and its action subcommands.
    /// @return The controller's own parser. Action parsers are stored in the same `subparsers`
    ///         vector, so callers must use this pointer, not a position in the vector.
    argparse::ArgumentParser *RegisterControllerArguments(const DataController &controller, argparse::ArgumentParser &program,
        std::vector<std::unique_ptr<argparse::ArgumentParser>> &subparsers)
    {
        const auto &columns = controller.GetColumns();

        // argparse stores references, so every parser must outlive `program`; unique_ptr because
        // ArgumentParser is neither copyable nor movable. exit_on_default_arguments is disabled so
        // --help does not std::exit() and destructors still run.
        subparsers.push_back(std::make_unique<argparse::ArgumentParser>(
            ToCommandName(controller.GetControllerName()), PSERV_VERSION_STRING, argparse::default_arguments::help, false));
        auto &cmd = *subparsers.back();
        cmd.add_description("Manage " + std::string(controller.GetItemName()) + "s");

        cmd.add_argument("--format").help("Output format: table, json, csv").default_value(std::string("table"));
        cmd.add_argument("--filter")
            .help("Filter results by text (case-insensitive substring match across all fields)")
            .default_value(std::string(""));
        cmd.add_argument("--sort").help("Sort by column name (ascending by default)").default_value(std::string(""));
        cmd.add_argument("--desc").help("Sort in descending order (use with --sort)").default_value(false).implicit_value(true);

        for (const auto &col : columns)
        {
            cmd.add_argument("--col-" + utils::ToLower(col.BindingName))
                .help("Filter by " + col.DisplayName + " (case-insensitive substring match)")
                .default_value(std::string(""));
        }

        for (const DataAction *action : controller.GetAllActions())
        {
            if (action->IsSeparator())
                continue;

            subparsers.push_back(std::make_unique<argparse::ArgumentParser>(
                ToCommandName(action->GetName()), PSERV_VERSION_STRING, argparse::default_arguments::help, false));
            auto &action_cmd = *subparsers.back();

            // The console cannot ask, so confirmation is given up front with --force.
            const bool bNeedsForce = action->IsDestructive() || action->RequiresConfirmation();
            std::string description = action->GetName();
            if (bNeedsForce)
            {
                description += " (DESTRUCTIVE - requires --force)";
            }
            action_cmd.add_description(description);

            // nargs(any), not remaining(): remaining() swallowed every later token, so the documented
            // `pservc services stop Spooler --force` treated "--force" as a target name.
            action_cmd.add_argument("targets")
                .help("Target object(s) to act upon (by " + columns[0].DisplayName + ")")
                .nargs(argparse::nargs_pattern::any);

            if (bNeedsForce)
            {
                action_cmd.add_argument("--force").help("Confirm the action without a prompt").default_value(false).implicit_value(true);
            }

            // The action's parameters become required --<name> options.
            for (const auto &param : action->GetParameters(&controller))
            {
                action_cmd.add_argument("--" + param.Name)
                    .help(param.Kind == ActionParameterKind::SaveFilePath ? param.Label + " (file path)" : param.Label)
                    .required();
            }

            cmd.add_subparser(action_cmd);
        }

        program.add_subparser(cmd);
        return &cmd;
    }
} // namespace

int main(int argc, char *argv[])
{
    utils::BaseApp baseApp;

    console::write_line(CONSOLE_FOREGROUND_GREEN "*** pservc " PSERV_VERSION_STRING " ***" CONSOLE_STANDARD);
    DataControllerLibrary dataControllerLibrary;
    // Disable exit_on_default_arguments to prevent std::exit() on --help/--version
    // This allows destructors to run properly and prevents memory leak reports
    argparse::ArgumentParser program{"pservc", PSERV_VERSION_STRING, argparse::default_arguments::all, false};

    // Storage for every parser (controllers and actions) - must outlive the program ArgumentParser
    std::vector<std::unique_ptr<argparse::ArgumentParser>> subparsers;
    std::vector<std::pair<DataController *, argparse::ArgumentParser *>> controllerParsers;

    for (const auto controller : dataControllerLibrary.GetDataControllers())
    {
        controllerParsers.emplace_back(controller, RegisterControllerArguments(*controller, program, subparsers));
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
    for (const auto &[controller, parser] : controllerParsers)
    {
        if (program.is_subcommand_used(ToCommandName(controller->GetControllerName())))
        {
            selectedController = controller;
            selectedSubparser = parser;
            break;
        }
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
    const DataAction *selectedAction = nullptr;
    argparse::ArgumentParser *selectedActionParser = nullptr;
    for (const DataAction *action : selectedController->GetAllActions())
    {
        if (action->IsSeparator())
            continue;

        const std::string action_name = ToCommandName(action->GetName());
        // is_subcommand_used() throws std::out_of_range for a name that is not registered
        try
        {
            if (selectedSubparser->is_subcommand_used(action_name))
            {
                selectedAction = action;
                selectedActionParser = &selectedSubparser->at<argparse::ArgumentParser>(action_name);
                break;
            }
        }
        catch (const std::exception &)
        {
            continue;
        }
    }

    // If action was selected, dispatch to action execution
    if (selectedAction && selectedActionParser)
    {
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

        // Actions that take their input from parameters (e.g. "add user variable") need no target;
        // every other action operates on named objects.
        if (targetNames.empty() && selectedAction->GetParameters(selectedController).empty())
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

        // The console cannot ask: actions that are destructive or need confirmation require --force
        if (selectedAction->IsDestructive() || selectedAction->RequiresConfirmation())
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

        // Create dispatch context. SetSelection retains the objects: the context owns one
        // reference per selected object and releases them when it is destroyed.
        ConsoleActionHost host;
        DataActionDispatchContext ctx;
        ctx.SetSelection(selectedObjects);
        ctx.m_pController = selectedController;
        ctx.m_pHost = &host;
        for (const auto &param : selectedAction->GetParameters(selectedController))
        {
            ctx.m_parameters[param.Name] = selectedActionParser->get<std::string>("--" + param.Name);
        }

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
                const auto status = ctx.m_pAsyncOp->GetStatus();
                const std::string error = ctx.m_pAsyncOp->GetErrorMessage();

                // Clean up async operation
                delete ctx.m_pAsyncOp;
                ctx.m_pAsyncOp = nullptr;

                if (status == AsyncStatus::Failed)
                {
                    console::write_line(CONSOLE_FOREGROUND_RED "Error executing action: " + error + CONSOLE_STANDARD);
                    return 1;
                }
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
            try
            {
                std::string filterValue = selectedSubparser->get<std::string>(argName);
                if (!filterValue.empty())
                {
                    columnFilters[static_cast<int>(i)] = filterValue;
                    spdlog::info("Column filter applied: column={} ({}), filter='{}'", i, columns[i].DisplayName, filterValue);
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
