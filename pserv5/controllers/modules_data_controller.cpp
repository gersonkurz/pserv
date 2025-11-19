#include "precomp.h"
#include "modules_data_controller.h"
#include "../windows_api/process_manager.h"
#include "../models/process_info.h"
#include "../utils/string_utils.h"
#include <spdlog/spdlog.h>
#include <format>
#include <algorithm>
#include <imgui.h>

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
        std::vector<ModuleInfo*> processModules = ModuleManager::EnumerateModules(proc->GetPid());

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

const std::vector<DataObject*>& ModulesDataController::GetDataObjects() const {
    return reinterpret_cast<const std::vector<DataObject*>&>(m_modules);
}

std::vector<int> ModulesDataController::GetAvailableActions(const DataObject* dataObject) const {
    std::vector<int> actions;
    if (!dataObject) return actions;

    actions.push_back(static_cast<int>(ModuleAction::CopyInfo));
    actions.push_back(static_cast<int>(ModuleAction::OpenContainingFolder));
    actions.push_back(static_cast<int>(ModuleAction::Separator));
    // actions.push_back(static_cast<int>(ModuleAction::Properties)); // TBD

    return actions;
}

std::string ModulesDataController::GetActionName(int action) const {
    switch (static_cast<ModuleAction>(action)) {
        case ModuleAction::CopyInfo: return "Copy Info";
        case ModuleAction::OpenContainingFolder: return "Open Containing Folder";
        // case ModuleAction::Properties: return "Properties";
        default: return "";
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
        case ModuleAction::CopyInfo: {
            // Build a string with module info to copy
            std::string info = std::format(
                "Name: {}\n" 
                "Path: {}\n" 
                "Base Address: {:#x}\n" 
                "Size: {}\n" 
                "Process ID: {}",
                module->GetName(),
                module->GetPath(),
                reinterpret_cast<uintptr_t>(module->GetBaseAddress()),
                module->GetSize(),
                module->GetProcessId()
            );
            ImGui::SetClipboardText(info.c_str());
            spdlog::info("Copied info for module: {}", module->GetName());
            break;
        }
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
    }
}

void ModulesDataController::RenderPropertiesDialog() {
    // No properties dialog for modules yet
}

void ModulesDataController::Sort(int columnIndex, bool ascending) {
    if (columnIndex < 0 || columnIndex >= static_cast<int>(GetColumns().size())) return;

    const auto& columns = GetColumns();
    ColumnDataType dataType = columns[columnIndex].DataType;

    std::sort(m_modules.begin(), m_modules.end(),
        [columnIndex, ascending, dataType](const ModuleInfo* a, const ModuleInfo* b) {
        // Use column metadata to determine sorting strategy
        if (dataType == ColumnDataType::UnsignedInteger) {
            uint64_t valA = 0, valB = 0;

            // 0=BaseAddress, 4=ProcessID
            switch (columnIndex) {
                case 0: valA = reinterpret_cast<uintptr_t>(a->GetBaseAddress());
                        valB = reinterpret_cast<uintptr_t>(b->GetBaseAddress()); break;
                case 4: valA = a->GetProcessId(); valB = b->GetProcessId(); break;
                default: valA = 0; valB = 0; break;
            }

            return ascending ? (valA < valB) : (valA > valB);
        }
        else if (dataType == ColumnDataType::Size) {
            // 1=Size
            uint64_t sizeA = a->GetSize();
            uint64_t sizeB = b->GetSize();
            return ascending ? (sizeA < sizeB) : (sizeA > sizeB);
        }

        // String comparison for all other types
        std::string valA = a->GetProperty(columnIndex);
        std::string valB = b->GetProperty(columnIndex);
        int cmp = valA.compare(valB);
        return ascending ? (cmp < 0) : (cmp > 0);
    });
}

} // namespace pserv
