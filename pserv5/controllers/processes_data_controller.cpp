#include "precomp.h"
#include "processes_data_controller.h"
#include "../windows_api/process_manager.h"
#include "../core/async_operation.h"
#include "../utils/string_utils.h"
#include <shellapi.h>

namespace pserv {

ProcessesDataController::ProcessesDataController()
    : DataController("Processes", "Process")
    , m_pPropertiesDialog(new ProcessPropertiesDialog())
{
    m_columns = {
        DataObjectColumn("Name", "Name"),
        DataObjectColumn("PID", "PID"),
        DataObjectColumn("User", "User"),
        DataObjectColumn("Priority", "Priority"),
        DataObjectColumn("Threads", "ThreadCount"),
        DataObjectColumn("Working Set", "WorkingSetSize"),
        DataObjectColumn("Private Bytes", "PrivatePageCount"),
        DataObjectColumn("Path", "Path"),
        DataObjectColumn("Command Line", "CommandLine"),
        DataObjectColumn("Handles", "HandleCount"),
        DataObjectColumn("Start Time", "StartTime"),
        DataObjectColumn("CPU Time", "TotalCPUTime"),
        DataObjectColumn("Kernel Time", "KernelCPUTime"),
        DataObjectColumn("User Time", "UserCPUTime"),
        DataObjectColumn("Paged Pool", "PagedPoolUsage"),
        DataObjectColumn("Non-Paged Pool", "NonPagedPoolUsage"),
        DataObjectColumn("Page Faults", "PageFaultCount")
    };
}

ProcessesDataController::~ProcessesDataController() {
    Clear();
    delete m_pPropertiesDialog;
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
        ProcessManager pm;
        m_processes = pm.EnumerateProcesses();
        
        // Re-apply sort
        if (m_lastSortColumn >= 0) {
            Sort(m_lastSortColumn, m_lastSortAscending);
        }
        
        spdlog::info("Refreshed {} processes", m_processes.size());
        m_bLoaded = true;
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to refresh processes: {}", e.what());
    }
}

void ProcessesDataController::Clear() {
    for (auto* p : m_processes) {
        delete p;
    }
    m_processes.clear();
    m_bLoaded = false;
}

const std::vector<DataObjectColumn>& ProcessesDataController::GetColumns() const {
    return m_columns;
}

std::vector<int> ProcessesDataController::GetAvailableActions(const DataObject* dataObject) const {
    std::vector<int> actions;
    if (!dataObject) return actions;

    actions.push_back(static_cast<int>(ProcessAction::Properties));
    actions.push_back(static_cast<int>(ProcessAction::Separator));

    actions.push_back(static_cast<int>(ProcessAction::OpenLocation));
    actions.push_back(static_cast<int>(ProcessAction::Separator));
    
    actions.push_back(static_cast<int>(ProcessAction::SetPriorityRealtime));
    actions.push_back(static_cast<int>(ProcessAction::SetPriorityHigh));
    actions.push_back(static_cast<int>(ProcessAction::SetPriorityAboveNormal));
    actions.push_back(static_cast<int>(ProcessAction::SetPriorityNormal));
    actions.push_back(static_cast<int>(ProcessAction::SetPriorityBelowNormal));
    actions.push_back(static_cast<int>(ProcessAction::SetPriorityLow));
    
    actions.push_back(static_cast<int>(ProcessAction::Separator));
    actions.push_back(static_cast<int>(ProcessAction::Terminate));

    return actions;
}

std::string ProcessesDataController::GetActionName(int action) const {
    switch (static_cast<ProcessAction>(action)) {
        case ProcessAction::Properties: return "Properties";
        case ProcessAction::Terminate: return "Terminate Process";
        case ProcessAction::OpenLocation: return "Open File Location";
        case ProcessAction::SetPriorityRealtime: return "Set Priority: Realtime";
        case ProcessAction::SetPriorityHigh: return "Set Priority: High";
        case ProcessAction::SetPriorityAboveNormal: return "Set Priority: Above Normal";
        case ProcessAction::SetPriorityNormal: return "Set Priority: Normal";
        case ProcessAction::SetPriorityBelowNormal: return "Set Priority: Below Normal";
        case ProcessAction::SetPriorityLow: return "Set Priority: Low";
        default: return "";
    }
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

void ProcessesDataController::RenderPropertiesDialog() {
    if (m_pPropertiesDialog && m_pPropertiesDialog->IsOpen()) {
        m_pPropertiesDialog->Render();
    }
}

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
}

void ProcessesDataController::Sort(int columnIndex, bool ascending) {
    if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size())) return;

    m_lastSortColumn = columnIndex;
    m_lastSortAscending = ascending;

    std::sort(m_processes.begin(), m_processes.end(), [columnIndex, ascending](const ProcessInfo* a, const ProcessInfo* b) {
        // Handle numeric columns
        // PID=1, Threads=4, Handles=9, PageFaults=16
        // Memory columns (5,6,14,15) are currently string-sorted (TODO: fix)
        if (columnIndex == 1 || columnIndex == 4 || columnIndex == 9 || columnIndex == 16) {
             long long valA = 0, valB = 0;
             try {
                 // This is inefficient (parsing back string), but consistent with current architecture
                 // Better way: Add GetPropertyAsLong to DataObject or specialized sort in controller
                 // For now, parsing is "safe" enough for prototype
                 std::string sA = a->GetProperty(columnIndex);
                 std::string sB = b->GetProperty(columnIndex);
                 
                 // Clean up " KB", " MB" etc for size columns if present
                 // Actually GetProperty returns formatted string.
                 // Ideally we should sort by raw value.
                 // We can cast to ProcessInfo and get raw values since we are in ProcessesDataController
                 
                 switch(columnIndex) {
                     case 1: valA = a->GetPid(); valB = b->GetPid(); break;
                     case 4: valA = a->GetThreadCount(); valB = b->GetThreadCount(); break;
                     case 9: valA = a->GetHandleCount(); valB = b->GetHandleCount(); break;
                     case 16: valA = a->GetPageFaultCount(); valB = b->GetPageFaultCount(); break;
                 }
             } catch (...) {}
             
             return ascending ? (valA < valB) : (valA > valB);
        }
        
        std::string valA = a->GetProperty(columnIndex);
        std::string valB = b->GetProperty(columnIndex);
        int cmp = valA.compare(valB);
        return ascending ? (cmp < 0) : (cmp > 0);
    });
}

} // namespace pserv