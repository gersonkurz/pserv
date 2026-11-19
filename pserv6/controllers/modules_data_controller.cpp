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
              {{ModuleProperty::BaseAddress, "Base Address", "Base Address", ColumnDataType::UnsignedInteger},
                  {ModuleProperty::Size, "Size", "Size", ColumnDataType::Size},
                  {ModuleProperty::Name, "Name", "Name", ColumnDataType::String},
                  {ModuleProperty::Path, "Path", "Path", ColumnDataType::String},
                  {ModuleProperty::ProcessId, "Process ID", "ProcessID", ColumnDataType::UnsignedInteger}}}
    {
    }

    void ModulesDataController::Collect(DataObjectContainer &target, const RefreshRequest &request)
    {
        // Modules are enumerated per process
        DataObjectContainer processes;
        ProcessManager::EnumerateProcesses(&processes);
        target.StartRefresh();
        for (auto proc : processes)
        {
            ModuleManager::EnumerateModules(&target, static_cast<ProcessInfo *>(proc)->GetPid());
        }
        target.FinishRefresh();
    }

    std::vector<const DataAction *> ModulesDataController::GetActions(const DataObject *dataObject) const
    {
        return CreateModuleActions();
    }

    std::vector<const DataAction *> ModulesDataController::GetAllActions() const
    {
        return CreateAllModuleActions();
    }

    VisualState ModulesDataController::GetVisualState(const DataObject *dataObject) const
    {
        // Modules don't have complex visual states like services or processes, so always normal.
        return VisualState::Normal;
    }    

} // namespace pserv
