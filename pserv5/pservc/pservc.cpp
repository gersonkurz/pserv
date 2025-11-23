#include "precomp.h"
#include <core/data_controller.h>
#include <core/data_controller_library.h>
#include <argparse/argparse.hpp>

using namespace pserv;

int main(int argc, char *argv[])
{
    DataControllerLibrary dataControllerLibrary;
    argparse::ArgumentParser program{"pservc"};

    for (const auto controller : dataControllerLibrary.GetDataControllers())
    {
        controller->RegisterArguments(program);
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
