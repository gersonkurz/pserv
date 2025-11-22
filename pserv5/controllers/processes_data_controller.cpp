#include "precomp.h"
#include <actions/process_actions.h>
#include <controllers/processes_data_controller.h>
#include <core/async_operation.h>
#include <models/process_info.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/process_manager.h>

namespace pserv
{

    ProcessesDataController::ProcessesDataController()
        : DataController{"Processes",
              "Process",
              {{"Name", "Name", ColumnDataType::String},
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
                  {"Page Faults", "PageFaultCount", ColumnDataType::UnsignedInteger}}}
    {
    }

    void ProcessesDataController::Refresh()
    {
        spdlog::info("Refreshing processes...");
        Clear();

        // Query current user name (always, in case of user switch)
        m_currentUserName.clear();
        char buffer[256];
        DWORD size = sizeof(buffer);
        if (GetUserNameA(buffer, &size))
        {
            m_currentUserName = buffer;
        }
        else
        {
            LogExpectedWin32Error("GetUserNameA");
        }

        try
        {
            m_objects = ProcessManager::EnumerateProcesses();

            // Re-apply sort
            if (m_lastSortColumn >= 0)
            {
                Sort(m_lastSortColumn, m_lastSortAscending);
            }

            spdlog::info("Refreshed {} processes", m_objects.size());
            m_bLoaded = true;
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to refresh processes: {}", e.what());
        }
    }

    std::vector<const DataAction *> ProcessesDataController::GetActions(const DataObject *dataObject) const
    {
        return CreateProcessActions();
    }

    VisualState ProcessesDataController::GetVisualState(const DataObject *dataObject) const
    {
        if (!dataObject)
            return VisualState::Normal;

        const auto *proc = static_cast<const ProcessInfo *>(dataObject);
        const std::string &user = proc->GetUser();

        // Disabled if SYSTEM
        if (user == "SYSTEM")
        {
            return VisualState::Disabled;
        }

        // Highlight if own process
        // Check if user ends with "\m_currentUserName" or equals m_currentUserName
        if (!m_currentUserName.empty())
        {
            if (user == m_currentUserName)
            {
                return VisualState::Highlighted;
            }

            // Check for DOMAIN\User format
            if (user.length() > m_currentUserName.length())
            {
                if (user.ends_with("\\" + m_currentUserName))
                {
                    return VisualState::Highlighted;
                }
            }
        }

        return VisualState::Normal;
    }
   
} // namespace pserv