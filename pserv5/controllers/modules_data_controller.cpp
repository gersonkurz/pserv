#include "precomp.h"
#include <controllers/modules_data_controller.h>
#include <windows_api/process_manager.h>
#include <models/process_info.h>
#include <utils/string_utils.h>
#include <actions/module_actions.h>

namespace pserv {

ModulesDataController::ModulesDataController()
    : DataController{"Modules", "Module", {
        {"Base Address", "Base Address", ColumnDataType::UnsignedInteger},
        {"Size", "Size", ColumnDataType::Size},
        {"Name", "Name", ColumnDataType::String},
        {"Path", "Path", ColumnDataType::String},
        {"Process ID", "ProcessID", ColumnDataType::UnsignedInteger}
    } }
{
}

void ModulesDataController::Refresh() {
    spdlog::info("Refreshing modules...");
    Clear();

    // Enumerate all processes to get their modules
    auto processes = ProcessManager::EnumerateProcesses();
    for (const auto proc : processes) {
        // Enumerate modules for this process
        const auto processModules{ ModuleManager::EnumerateModules(static_cast<ProcessInfo*>(proc)->GetPid()) };

        // Add to our main list
        m_objects.insert(m_objects.end(), processModules.begin(), processModules.end());

        // Modules are owned by this controller now, so delete process info object.
        delete proc;
    }
    spdlog::info("Refreshed {} modules from {} processes", m_objects.size(), processes.size());
    processes.clear(); // Clear the vector of invalidated pointers
    m_bLoaded = true;
}


std::vector<const DataAction*> ModulesDataController::GetActions(const DataObject* dataObject) const {
    return CreateModuleActions();
}

VisualState ModulesDataController::GetVisualState(const DataObject* dataObject) const {
    // Modules don't have complex visual states like services or processes, so always normal.
    return VisualState::Normal;
}
/*
void ModulesDataController::DispatchAction(int action, DataActionDispatchContext& context) {
    if (context.m_selectedObjects.empty()) return;

    auto modAction = static_cast<ModuleAction>(action);
    const ModuleInfo* module = static_cast<const ModuleInfo*>(context.m_selectedObjects[0]); // Actions typically on single selection

    switch (modAction) {
        case ModuleAction::OpenContainingFolder: {
            std::string path = module->GetPath();
            if (!path.empty()) {
                size_t lastSlash = path.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    std::string folder = path.substr(0, lastSlash);
                    ShellExecuteA(nullptr, "open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    spdlog::info("Opened containing folder for module: {}", module->GetName());
                } else {
                    spdlog::warn("Could not determine containing folder for module: {}", module->GetName());
                }
            } else {
                spdlog::warn("Module path is empty, cannot open containing folder: {}", module->GetName());
            }
            break;
        }
        // case ModuleAction::Properties: {
        //     // TBD: Open ModulePropertiesDialog
        //     break;
        // }

        default:
            // Delegate to base class for common actions
            DispatchCommonAction(action, context);
            break;
    }
}
*/

} // namespace pserv
