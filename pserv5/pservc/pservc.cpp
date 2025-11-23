#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_controller_library.h>
#include <argparse/argparse.hpp>
#include <pservc/console.h>
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
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }
    return 0;
}
