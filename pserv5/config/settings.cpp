#include "precomp.h"
#include <config/settings.h>
#include <core/data_controller.h>

namespace pserv
{
    namespace config
    {
        RootSettings theSettings;

        DisplayTable *RootSettings::getSectionFor(const std::string &name)
        {
            if (name == SERVICES_DATA_CONTROLLER_NAME)
            {
                return &servicesTable;
            }
            if (name == DEVICES_DATA_CONTROLLER_NAME)
            {
                return &devicesTable;
            }
            if (name == PROCESSES_DATA_CONTROLLER_NAME)
            {
                return &processesTable;
            }
            if (name == WINDOWS_DATA_CONTROLLER_NAME)
            {
                return &windowsTable;
            }
            if (name == MODULES_DATA_CONTROLLER_NAME)
            {
                return &modulesTable;
            }
            if (name == UNINSTALLER_DATA_CONTROLLER_NAME)
            {
                return &uninstallerTable;
            }
            if (name == ENVIRONMENT_VARIABLES_CONTROLLER_NAME)
            {
                return &environmentVariablesTable;
            }
            if (name == STARTUP_PROGRAMS_DATA_CONTROLLER_NAME)
            {
                return &startupProgramsTable;
            }
            if (name == NETWORK_CONNECTIONS_DATA_CONTROLLER_NAME)
            {
                return &networkConnectionsProperties;
            }
            if (name == SCHEDULED_TASKS_DATA_CONTROLLER_NAME)
            {
                return &scheduledTasksProperties;
            }
            return nullptr;
        }
    } // namespace config
} // namespace pserv
