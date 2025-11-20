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

ModulesDataController::~ModulesDataController() {
    Clear();
}

void ModulesDataController::Refresh() {
    spdlog::info("Refreshing modules...");
    Clear();

    // Enumerate all processes to get their modules
    std::vector<ProcessInfo*> processes = ProcessManager::EnumerateProcesses();

    for (ProcessInfo* proc : processes) {
        // Enumerate modules for this process
        auto processModules = ModuleManager::EnumerateModules(proc->GetPid());

        // Add to our main list
        m_modules.insert(m_modules.end(), processModules.begin(), processModules.end());

        // Modules are owned by this controller now, so delete process info object.
        delete proc;
    }
    processes.clear(); // Clear the vector of invalidated pointers

    spdlog::info("Refreshed {} modules from {} processes", m_modules.size(), processes.size());
    m_bLoaded = true;
}

void ModulesDataController::Clear() {
    for (auto* mod : m_modules) {
        delete mod;
    }
    m_modules.clear();
    m_bLoaded = false;
}

std::vector<std::shared_ptr<DataAction>> ModulesDataController::GetActions() const {
    return CreateModuleActions();
}

const std::vector<DataObject*>& ModulesDataController::GetDataObjects() const {
    return reinterpret_cast<const std::vector<DataObject*>&>(m_modules);
}

std::vector<int> ModulesDataController::GetAvailableActions(const DataObject* dataObject) const {
    std::vector<int> actions;
    if (!dataObject) return actions;

    actions.push_back(static_cast<int>(ModuleAction::OpenContainingFolder));

    // Add common export/copy actions
    AddCommonExportActions(actions);

    return actions;
}

std::string ModulesDataController::GetActionName(int action) const {
    switch (static_cast<ModuleAction>(action)) {
        case ModuleAction::OpenContainingFolder: return "Open Containing Folder";
        // case ModuleAction::Properties: return "Properties";
        default:
            std::string commonName = GetCommonActionName(action);
            return !commonName.empty() ? commonName : "";
    }
}

VisualState ModulesDataController::GetVisualState(const DataObject* dataObject) const {
    // Modules don't have complex visual states like services or processes, so always normal.
    return VisualState::Normal;
}

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

void ModulesDataController::RenderPropertiesDialog() {
    // No properties dialog for modules yet
}

} // namespace pserv
