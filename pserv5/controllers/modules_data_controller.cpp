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

    void ModulesDataController::Refresh(bool isAutoRefresh)
    {
        spdlog::info("Refreshing modules...");

        // Enumerate all processes to get their modules. Actually, this is more tricky than it seems, because
        // m_objects creates BOTH modules AND processes.
        // Note: We don't call Clear() here - StartRefresh/FinishRefresh handles
        // update-in-place for existing objects and removes stale ones
        DataObjectContainer processes;
        ProcessManager::EnumerateProcesses(&processes);
        m_objects.StartRefresh();
        for (auto proc : processes)
        {
            // Enumerate modules for this process
            ModuleManager::EnumerateModules(&m_objects, static_cast<ProcessInfo*>(proc)->GetPid());
        }
        m_objects.FinishRefresh();
        spdlog::info("Refreshed {} modules from {} processes", m_objects.GetSize(), processes.GetSize());
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
