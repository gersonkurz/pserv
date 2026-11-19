#include "pch.h"
#include "providers.h"
#include <controllers/devices_data_controller.h>
#include <controllers/processes_data_controller.h>
#include <controllers/windows_data_controller.h>
#include <controllers/modules_data_controller.h>
#include <controllers/uninstaller_data_controller.h>
#include <controllers/environment_variables_data_controller.h>
#include <controllers/startup_programs_data_controller.h>
#include <controllers/network_connections_data_controller.h>
#include <controllers/scheduled_tasks_data_controller.h>

namespace pserv::winui
{
    namespace
    {
        template<typename T> std::shared_ptr<DataController> Create() { return std::make_shared<T>(); }
    }
    const std::vector<Provider>& Providers()
    {
        static const std::vector<Provider> providers{
            {L"Services", L"\uE713", &Create<ServicesDataController>, {0,1,2,3,4,8,7}, {"Start", "Stop", "Restart"}},
            {L"Devices", L"\uE772", &Create<DevicesDataController>, {0,1,2,3,6}, {"Start", "Stop"}},
            {L"Processes", L"\uE9D9", &Create<ProcessesDataController>, {}, {"Terminate Process", "Open File Location"}},
            {L"Windows", L"\uE737", &Create<WindowsDataController>, {1,10,0,2,3,4}, {"Bring To Front", "Show", "Hide"}},
            {L"Modules", L"\uE74C", &Create<ModulesDataController>, {2,4,3,0,1}, {}},
            {L"Installed programs", L"\uE71D", &Create<UninstallerDataController>, {}, {"Uninstall"}},
            {L"Environment variables", L"\uE943", &Create<EnvironmentVariablesDataController>, {}, {}},
            {L"Startup programs", L"\uE7E8", &Create<StartupProgramsDataController>, {}, {"Enable", "Disable"}},
            {L"Network connections", L"\uE968", &Create<NetworkConnectionsDataController>, {}, {}},
            {L"Scheduled tasks", L"\uE787", &Create<ScheduledTasksDataController>, {}, {"Run Now", "Enable", "Disable"}}
        };
        return providers;
    }
}
