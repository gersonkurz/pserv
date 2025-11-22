#include "precomp.h"
#include <actions/module_actions.h>
#include <controllers/modules_data_controller.h>
#include <models/process_info.h>
#include <utils/string_utils.h>
#include <windows_api/process_manager.h>
#include <models/module_info.h>
#include <windows_api/module_manager.h>

namespace pserv
{

    ModulesDataController::ModulesDataController()
        : DataController{MODULES_DATA_CONTROLLER_NAME,
              "Module",
              {{"Base Address", "Base Address", ColumnDataType::UnsignedInteger},
                  {"Size", "Size", ColumnDataType::Size},
                  {"Name", "Name", ColumnDataType::String},
                  {"Path", "Path", ColumnDataType::String},
                  {"Process ID", "ProcessID", ColumnDataType::UnsignedInteger}}}
    {
    }

    void ModulesDataController::Refresh()
    {
        spdlog::info("Refreshing modules...");
        Clear();

        // Enumerate all processes to get their modules
        auto processes = ProcessManager::EnumerateProcesses();
        for (const auto proc : processes)
        {
            // Enumerate modules for this process
            const auto processModules{ModuleManager::EnumerateModules(static_cast<ProcessInfo *>(proc)->GetPid())};

            // Add to our main list
            m_objects.insert(m_objects.end(), processModules.begin(), processModules.end());

            // Modules are owned by this controller now, so delete process info object.
            delete proc;
        }
        spdlog::info("Refreshed {} modules from {} processes", m_objects.size(), processes.size());
        processes.clear(); // Clear the vector of invalidated pointers
        m_bLoaded = true;
    }

    std::vector<const DataAction *> ModulesDataController::GetActions(const DataObject *dataObject) const
    {
        return CreateModuleActions();
    }

    VisualState ModulesDataController::GetVisualState(const DataObject *dataObject) const
    {
        // Modules don't have complex visual states like services or processes, so always normal.
        return VisualState::Normal;
    }    

} // namespace pserv
