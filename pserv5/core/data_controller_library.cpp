#include "precomp.h"
#include <controllers/devices_data_controller.h>
#include <controllers/modules_data_controller.h>
#include <controllers/processes_data_controller.h>
#include <controllers/services_data_controller.h>
#include <controllers/uninstaller_data_controller.h>
#include <controllers/windows_data_controller.h>
#include <core/data_controller_library.h>

namespace
{
    std::vector<pserv::DataController *> dataControllers;
}

namespace pserv
{
    DataControllerLibrary::~DataControllerLibrary()
    {
        for (const auto ptr : dataControllers)
        {
            delete ptr;
        }
        dataControllers.clear();
    }

    const std::vector<DataController *> &DataControllerLibrary::GetDataControllers()
    {
        // layz initialization
        if (dataControllers.empty())
        {
            dataControllers.push_back(new ServicesDataController());
            dataControllers.push_back(new DevicesDataController());
            dataControllers.push_back(new ProcessesDataController());
            dataControllers.push_back(new WindowsDataController());
            dataControllers.push_back(new ModulesDataController());
            dataControllers.push_back(new UninstallerDataController());
        }
        return dataControllers;
    }
} // namespace pserv