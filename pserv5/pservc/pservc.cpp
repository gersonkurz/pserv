#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_controller_library.h>
#include <argparse/argparse.hpp>
#include <pservc/console.h>
#include <pservc/console_table.h>
#include <utils/logging.h>
#include <utils/base_app.h>

using namespace pserv;

int main(int argc, char *argv[])
{
    utils::BaseApp baseApp;

    console::write_line(CONSOLE_FOREGROUND_GREEN "*** pservc 5.0.0 ***" CONSOLE_STANDARD);
    DataControllerLibrary dataControllerLibrary;
    // Disable exit_on_default_arguments to prevent std::exit() on --help/--version
    // This allows destructors to run properly and prevents memory leak reports
    argparse::ArgumentParser program{"pservc", "5.0.0", argparse::default_arguments::all, false};

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

    // If --help or --version was provided, just exit
    if (program.get<bool>("--help") || program.get<bool>("--version"))
    {
        console::write(program.help().str());
        return 0;
    }

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

    // If no subcommand was provided, show help
    if (!selectedController || !selectedSubparser)
    {
        console::write(program.help().str());
        return 0;
    }

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

        // Sort by first column ascending (default behavior for predictable output)
        // User can override with --sort argument in the future
        console::write_line("Sorting by first column...");
        selectedController->Sort(0, true);
        console::write_line("Sorting complete");

        // Render the data
        console::ConsoleTable table(selectedController, format);
        table.Render(selectedController->GetDataObjects());
    }
    catch (const std::exception &err)
    {
        console::write_line(CONSOLE_FOREGROUND_RED "Error: " + std::string(err.what()) + CONSOLE_STANDARD);
        return 1;
    }

    return 0;
}
