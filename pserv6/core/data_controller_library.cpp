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
        Clear();
    }

    void DataControllerLibrary::Clear()
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
            dataControllers.push_back(DBG_NEW ServicesDataController());
            dataControllers.push_back(DBG_NEW DevicesDataController());
            dataControllers.push_back(DBG_NEW ProcessesDataController());
            dataControllers.push_back(DBG_NEW WindowsDataController());
            dataControllers.push_back(DBG_NEW ModulesDataController());
            dataControllers.push_back(DBG_NEW UninstallerDataController());
            dataControllers.push_back(DBG_NEW EnvironmentVariablesDataController());
            dataControllers.push_back(DBG_NEW StartupProgramsDataController());
            dataControllers.push_back(DBG_NEW NetworkConnectionsDataController());
            dataControllers.push_back(DBG_NEW ScheduledTasksDataController());
        }
        return dataControllers;
    }

    std::unique_ptr<DataController> DataControllerLibrary::CreateFreshController(const DataController *existing)
    {
        if (!existing)
        {
            return nullptr;
        }

        const auto &name = existing->GetControllerName();
        std::unique_ptr<DataController> fresh;
        if (name == SERVICES_DATA_CONTROLLER_NAME)
        {
            auto ctrl = std::make_unique<ServicesDataController>();
            if (const auto *svc = dynamic_cast<const ServicesDataController *>(existing))
            {
                ctrl->SetMachineName(svc->GetMachineName());
            }
            fresh = std::move(ctrl);
        }
        else if (name == DEVICES_DATA_CONTROLLER_NAME)
        {
            auto ctrl = std::make_unique<DevicesDataController>();
            if (const auto *svc = dynamic_cast<const ServicesDataController *>(existing))
            {
                ctrl->SetMachineName(svc->GetMachineName());
            }
            fresh = std::move(ctrl);
        }
        else if (name == PROCESSES_DATA_CONTROLLER_NAME)
        {
            fresh = std::make_unique<ProcessesDataController>();
        }
        else if (name == WINDOWS_DATA_CONTROLLER_NAME)
        {
            fresh = std::make_unique<WindowsDataController>();
        }
        else if (name == MODULES_DATA_CONTROLLER_NAME)
        {
            fresh = std::make_unique<ModulesDataController>();
        }
        else if (name == UNINSTALLER_DATA_CONTROLLER_NAME)
        {
            fresh = std::make_unique<UninstallerDataController>();
        }
        else if (name == ENVIRONMENT_VARIABLES_CONTROLLER_NAME)
        {
            fresh = std::make_unique<EnvironmentVariablesDataController>();
        }
        else if (name == STARTUP_PROGRAMS_DATA_CONTROLLER_NAME)
        {
            fresh = std::make_unique<StartupProgramsDataController>();
        }
        else if (name == NETWORK_CONNECTIONS_DATA_CONTROLLER_NAME)
        {
            fresh = std::make_unique<NetworkConnectionsDataController>();
        }
        else if (name == SCHEDULED_TASKS_DATA_CONTROLLER_NAME)
        {
            fresh = std::make_unique<ScheduledTasksDataController>();
        }

        if (fresh)
        {
            fresh->CopySortStateFrom(*existing);
        }
        return fresh;
    }
} // namespace pserv