#include "precomp.h"
#include <controllers/devices_data_controller.h>
#include <controllers/environment_variables_data_controller.h>
#include <controllers/modules_data_controller.h>
#include <controllers/network_connections_data_controller.h>
#include <controllers/processes_data_controller.h>
#include <controllers/scheduled_tasks_data_controller.h>
#include <controllers/services_data_controller.h>
#include <controllers/startup_programs_data_controller.h>
#include <controllers/uninstaller_data_controller.h>
#include <controllers/windows_data_controller.h>
#include <core/data_controller_library.h>
#include <core/data_controller.h>
#include <core/data_object.h>
#include <core/data_object_column.h>

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
            dataControllers.push_back(new EnvironmentVariablesDataController());
            dataControllers.push_back(new StartupProgramsDataController());
            dataControllers.push_back(new NetworkConnectionsDataController());
            dataControllers.push_back(new ScheduledTasksDataController());
        }
        return dataControllers;
    }
} // namespace pserv