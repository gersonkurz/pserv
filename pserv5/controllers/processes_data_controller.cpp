 #include "precomp.h"
#include <controllers/processes_data_controller.h>
#include <windows_api/process_manager.h>
#include <core/async_operation.h>
#include <utils/string_utils.h>
#include <actions/process_actions.h>

namespace pserv {

ProcessesDataController::ProcessesDataController()
    : DataController{"Processes", "Process", {
        {"Name", "Name", ColumnDataType::String},
        {"PID", "PID", ColumnDataType::UnsignedInteger},
        {"User", "User", ColumnDataType::String},
        {"Priority", "Priority", ColumnDataType::String},
        {"Threads", "ThreadCount", ColumnDataType::UnsignedInteger},
        {"Working Set", "WorkingSetSize", ColumnDataType::Size},
        {"Private Bytes", "PrivatePageCount", ColumnDataType::Size},
        {"Path", "Path", ColumnDataType::String},
        {"Command Line", "CommandLine", ColumnDataType::String},
        {"Handles", "HandleCount", ColumnDataType::UnsignedInteger},
        {"Start Time", "StartTime", ColumnDataType::Time},
        {"CPU Time", "TotalCPUTime", ColumnDataType::Time},
        {"Kernel Time", "KernelCPUTime", ColumnDataType::Time},
        {"User Time", "UserCPUTime", ColumnDataType::Time},
        {"Paged Pool", "PagedPoolUsage", ColumnDataType::Size},
        {"Non-Paged Pool", "NonPagedPoolUsage", ColumnDataType::Size},
        {"Page Faults", "PageFaultCount", ColumnDataType::UnsignedInteger}
    } }
{
}

void ProcessesDataController::Refresh() {
    spdlog::info("Refreshing processes...");
    Clear();

    // Query current user name (always, in case of user switch)
    m_currentUserName.clear();
    char buffer[256];
    DWORD size = sizeof(buffer);
    if (GetUserNameA(buffer, &size)) {
        m_currentUserName = buffer;
    }

    try {
        m_objects = ProcessManager::EnumerateProcesses();
        
        // Re-apply sort
        if (m_lastSortColumn >= 0) {
            Sort(m_lastSortColumn, m_lastSortAscending);
        }
        
        spdlog::info("Refreshed {} processes", m_objects.size());
        m_bLoaded = true;
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to refresh processes: {}", e.what());
    }
}

std::vector<const DataAction*> ProcessesDataController::GetActions(const DataObject* dataObject) const {
    return CreateProcessActions();
}

VisualState ProcessesDataController::GetVisualState(const DataObject* dataObject) const {
    if (!dataObject) return VisualState::Normal;
    
    const auto* proc = static_cast<const ProcessInfo*>(dataObject);
    const std::string& user = proc->GetUser();

    // Disabled if SYSTEM
    if (user == "SYSTEM") {
        return VisualState::Disabled;
    }
    
    // Highlight if own process
    // Check if user ends with "\m_currentUserName" or equals m_currentUserName
    if (!m_currentUserName.empty()) {
        if (user == m_currentUserName) {
            return VisualState::Highlighted;
        }
        
        // Check for DOMAIN\User format
        if (user.length() > m_currentUserName.length()) {
             if (user.ends_with("\\" + m_currentUserName)) {
                 return VisualState::Highlighted;
             }
        }
    }

    return VisualState::Normal;
}
/*
void ProcessesDataController::DispatchAction(int action, DataActionDispatchContext& context) {
    if (context.m_selectedObjects.empty()) return;

    auto processAction = static_cast<ProcessAction>(action);

    if (processAction == ProcessAction::Properties) {
        std::vector<ProcessInfo*> selected;
        for (const auto* obj : context.m_selectedObjects) {
            selected.push_back(const_cast<ProcessInfo*>(static_cast<const ProcessInfo*>(obj)));
        }
        m_pPropertiesDialog->Open(selected);
    }
    else if (processAction == ProcessAction::Terminate) {
        std::vector<DWORD> pids;
        std::string confirmMsg = "Are you sure you want to terminate the following processes?\n\n";
        
        for (const auto* obj : context.m_selectedObjects) {
            const auto* proc = static_cast<const ProcessInfo*>(obj);
            pids.push_back(proc->GetPid());
            confirmMsg += std::format("{} (PID: {})\n", proc->GetName(), proc->GetPid());
            if (pids.size() >= 10) {
                confirmMsg += "... and more\n";
                break;
            }
        }

        if (MessageBoxA(context.m_hWnd, confirmMsg.c_str(), "Confirm Termination", MB_YESNO | MB_ICONWARNING) == IDYES) {
            
             // Async termination
            if (context.m_pAsyncOp) {
                context.m_pAsyncOp->Wait();
                delete context.m_pAsyncOp;
                context.m_pAsyncOp = nullptr;
            }

            context.m_pAsyncOp = new AsyncOperation();
            context.m_bShowProgressDialog = true;

            context.m_pAsyncOp->Start(context.m_hWnd, [pids](AsyncOperation* op) -> bool {
                size_t total = pids.size();
                size_t success = 0;
                for (size_t i = 0; i < total; ++i) {
                    op->ReportProgress(static_cast<float>(i) / total, std::format("Terminating process PID {}...", pids[i]));
                    if (ProcessManager::TerminateProcessById(pids[i])) {
                        success++;
                    }
                }
                op->ReportProgress(1.0f, std::format("Terminated {}/{} processes", success, total));
                return true;
            });
        }
    }
    else if (processAction == ProcessAction::OpenLocation) {
        for (const auto* obj : context.m_selectedObjects) {
            const auto* proc = static_cast<const ProcessInfo*>(obj);
            std::string path = proc->GetPath();
            if (!path.empty()) {
                // Select file in explorer
                std::wstring wPath = utils::Utf8ToWide(path);
                std::wstring cmd = L"/select,\"" + wPath + L"\"";
                ShellExecuteW(NULL, L"open", L"explorer.exe", cmd.c_str(), NULL, SW_SHOW);
            }
        }
    }
    else {
        // Set Priority
        DWORD priorityClass = NORMAL_PRIORITY_CLASS;
        switch (processAction) {
            case ProcessAction::SetPriorityRealtime: priorityClass = REALTIME_PRIORITY_CLASS; break;
            case ProcessAction::SetPriorityHigh: priorityClass = HIGH_PRIORITY_CLASS; break;
            case ProcessAction::SetPriorityAboveNormal: priorityClass = ABOVE_NORMAL_PRIORITY_CLASS; break;
            case ProcessAction::SetPriorityNormal: priorityClass = NORMAL_PRIORITY_CLASS; break;
            case ProcessAction::SetPriorityBelowNormal: priorityClass = BELOW_NORMAL_PRIORITY_CLASS; break;
            case ProcessAction::SetPriorityLow: priorityClass = IDLE_PRIORITY_CLASS; break;
        }

        std::vector<DWORD> pids;
        for (const auto* obj : context.m_selectedObjects) {
            pids.push_back(static_cast<const ProcessInfo*>(obj)->GetPid());
        }

        // No async needed for priority usually, it's fast
        int success = 0;
        for (DWORD pid : pids) {
            if (ProcessManager::SetProcessPriority(pid, priorityClass)) {
                success++;
            }
        }
        spdlog::info("Set priority for {}/{} processes", success, pids.size());
        Refresh(); // Refresh to show new priority
    }

    // If not handled above, delegate to base class for common actions
    if (processAction != ProcessAction::Properties &&
        processAction != ProcessAction::Terminate &&
        processAction != ProcessAction::OpenLocation &&
        processAction != ProcessAction::SetPriorityRealtime &&
        processAction != ProcessAction::SetPriorityHigh &&
        processAction != ProcessAction::SetPriorityAboveNormal &&
        processAction != ProcessAction::SetPriorityNormal &&
        processAction != ProcessAction::SetPriorityBelowNormal &&
        processAction != ProcessAction::SetPriorityLow) {
        DispatchCommonAction(action, context);
    }
}
*/
} // namespace pserv