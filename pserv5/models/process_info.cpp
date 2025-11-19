#include "precomp.h"
#include "process_info.h"
#include <utils/string_utils.h>
#include <utils/format_utils.h>

namespace pserv {

ProcessInfo::ProcessInfo(DWORD pid, std::string name)
    : m_pid{pid}
    , m_name{std::move(name)}
{
    SetRunning(true);
}

PropertyValue ProcessInfo::GetTypedProperty(int propertyId) const {
    switch (static_cast<ProcessProperty>(propertyId)) {
        case ProcessProperty::Name:
        case ProcessProperty::User:
        case ProcessProperty::Priority:
        case ProcessProperty::Path:
        case ProcessProperty::CommandLine:
            return GetProperty(propertyId);

        case ProcessProperty::PID:
            return static_cast<uint64_t>(m_pid);
        case ProcessProperty::ParentPID:
            return static_cast<uint64_t>(m_parentPid);
        case ProcessProperty::ThreadCount:
            return static_cast<uint64_t>(m_threadCount);
        case ProcessProperty::HandleCount:
            return static_cast<uint64_t>(m_handleCount);
        case ProcessProperty::SessionId:
            return static_cast<uint64_t>(m_sessionId);
        case ProcessProperty::PageFaultCount:
            return static_cast<uint64_t>(m_pageFaultCount);

        case ProcessProperty::WorkingSetSize:
            return static_cast<uint64_t>(m_workingSetSize);
        case ProcessProperty::PeakWorkingSetSize:
            return static_cast<uint64_t>(m_peakWorkingSetSize);
        case ProcessProperty::PrivatePageCount:
            return static_cast<uint64_t>(m_privatePageCount);
        case ProcessProperty::VirtualSize:
            return static_cast<uint64_t>(m_virtualSize);
        case ProcessProperty::PagedPoolUsage:
            return static_cast<uint64_t>(m_quotaPagedPoolUsage);
        case ProcessProperty::NonPagedPoolUsage:
            return static_cast<uint64_t>(m_quotaNonPagedPoolUsage);

        case ProcessProperty::StartTime:
        case ProcessProperty::TotalCPUTime:
        case ProcessProperty::UserCPUTime:
        case ProcessProperty::KernelCPUTime:
            return GetProperty(propertyId);

        default:
            return std::monostate{};
    }
}

std::string ProcessInfo::GetProperty(int propertyId) const {
    switch (static_cast<ProcessProperty>(propertyId)) {
        case ProcessProperty::Name:
            return m_name;
        case ProcessProperty::PID:
            return std::to_string(m_pid);
        case ProcessProperty::ParentPID:
            return std::to_string(m_parentPid);
        case ProcessProperty::ThreadCount:
            return std::to_string(m_threadCount);
        case ProcessProperty::Priority:
            return GetPriorityString();
        case ProcessProperty::User:
            return m_user;
        case ProcessProperty::Path:
            return m_path;
        case ProcessProperty::CommandLine:
            return m_commandLine;
        case ProcessProperty::WorkingSetSize:
            return utils::FormatSize(m_workingSetSize);
        case ProcessProperty::PeakWorkingSetSize:
            return utils::FormatSize(m_peakWorkingSetSize);
        case ProcessProperty::PrivatePageCount:
            return utils::FormatSize(m_privatePageCount);
        case ProcessProperty::VirtualSize:
            return utils::FormatSize(m_virtualSize);
        case ProcessProperty::HandleCount:
            return std::to_string(m_handleCount);
        case ProcessProperty::SessionId:
            return std::to_string(m_sessionId);
        case ProcessProperty::StartTime:
            return FileTimeToString(m_creationTime);
        case ProcessProperty::TotalCPUTime:
            {
                ULARGE_INTEGER kernel, user;
                kernel.LowPart = m_kernelTime.dwLowDateTime; kernel.HighPart = m_kernelTime.dwHighDateTime;
                user.LowPart = m_userTime.dwLowDateTime; user.HighPart = m_userTime.dwHighDateTime;
                ULARGE_INTEGER total;
                total.QuadPart = kernel.QuadPart + user.QuadPart;
                FILETIME ftTotal;
                ftTotal.dwLowDateTime = total.LowPart;
                ftTotal.dwHighDateTime = total.HighPart;
                return DurationToString(ftTotal);
            }
        case ProcessProperty::UserCPUTime:
            return DurationToString(m_userTime);
        case ProcessProperty::KernelCPUTime:
            return DurationToString(m_kernelTime);
        case ProcessProperty::PagedPoolUsage:
            return utils::FormatSize(m_quotaPagedPoolUsage);
        case ProcessProperty::NonPagedPoolUsage:
            return utils::FormatSize(m_quotaNonPagedPoolUsage);
        case ProcessProperty::PageFaultCount:
            return std::to_string(m_pageFaultCount);
        default:
            return "";
    }
}

bool ProcessInfo::MatchesFilter(const std::string& filter) const {
    if (filter.empty()) return true;
    
    // Case-insensitive search in Name, PID, User, Path
    std::string filterLower = utils::ToLower(filter);

    if (utils::ToLower(m_name).find(filterLower) != std::string::npos) return true;
    if (std::to_string(m_pid).find(filterLower) != std::string::npos) return true;
    if (utils::ToLower(m_user).find(filterLower) != std::string::npos) return true;
    if (utils::ToLower(m_path).find(filterLower) != std::string::npos) return true;

    return false;
}

std::string ProcessInfo::GetPriorityString() const {
    switch (m_priorityClass) {
        case IDLE_PRIORITY_CLASS: return "Idle";
        case BELOW_NORMAL_PRIORITY_CLASS: return "Below Normal";
        case NORMAL_PRIORITY_CLASS: return "Normal";
        case ABOVE_NORMAL_PRIORITY_CLASS: return "Above Normal";
        case HIGH_PRIORITY_CLASS: return "High";
        case REALTIME_PRIORITY_CLASS: return "Realtime";
        case 0: return "";
        default: return std::format("Unknown ({})", m_priorityClass);
    }
}

std::string ProcessInfo::FileTimeToString(const FILETIME& ft) {
    if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) return "";
    
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

    // Use Windows API for localized date/time string
    // Or simple snprintf for now: YYYY-MM-DD HH:MM:SS
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
        stLocal.wYear, stLocal.wMonth, stLocal.wDay,
        stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
        
    return std::string(buffer);
}

std::string ProcessInfo::DurationToString(const FILETIME& ft) {
    ULARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    
    // FILETIME is 100-nanosecond intervals
    // 1 second = 10,000,000 intervals
    uint64_t seconds = li.QuadPart / 10000000;
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%02llu:%02llu:%02llu", hours, minutes, seconds);
    return std::string(buffer);
}

} // namespace pserv
