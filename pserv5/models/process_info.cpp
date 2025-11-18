#include "precomp.h"
#include "process_info.h"
#include <utils/string_utils.h>

namespace pserv {

ProcessInfo::ProcessInfo(DWORD pid, std::string name)
    : m_pid{pid}
    , m_name{std::move(name)}
{
    SetRunning(true);
}

std::string ProcessInfo::GetId() const {
    // Unique ID is composed of PID and Name to handle PID reuse reuse cases
    // though strictly speaking PID is unique at any point in time.
    // Using just PID string is simpler for now.
    return std::to_string(m_pid);
}

void ProcessInfo::Update(const DataObject& other) {
    const auto& otherProcess = static_cast<const ProcessInfo&>(other);
    
    // Update all mutable properties
    m_parentPid = otherProcess.m_parentPid;
    m_threadCount = otherProcess.m_threadCount;
    m_priorityClass = otherProcess.m_priorityClass;
    m_user = otherProcess.m_user;
    m_path = otherProcess.m_path;
    m_commandLine = otherProcess.m_commandLine;
    m_workingSetSize = otherProcess.m_workingSetSize;
    m_peakWorkingSetSize = otherProcess.m_peakWorkingSetSize;
    m_privatePageCount = otherProcess.m_privatePageCount;
    m_virtualSize = otherProcess.m_virtualSize;
    m_handleCount = otherProcess.m_handleCount;
    m_sessionId = otherProcess.m_sessionId;

    // Name and PID are immutable for the same object ID usually, but update name just in case
    m_name = otherProcess.m_name;
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
            return BytesToSizeString(m_workingSetSize);
        case ProcessProperty::PeakWorkingSetSize:
            return BytesToSizeString(m_peakWorkingSetSize);
        case ProcessProperty::PrivatePageCount:
            return BytesToSizeString(m_privatePageCount);
        case ProcessProperty::VirtualSize:
            return BytesToSizeString(m_virtualSize);
        case ProcessProperty::HandleCount:
            return std::to_string(m_handleCount);
        case ProcessProperty::SessionId:
            return std::to_string(m_sessionId);
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

std::string ProcessInfo::BytesToSizeString(SIZE_T bytes) {
    const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
    int suffixIndex = 0;
    double doubleBytes = static_cast<double>(bytes);

    while (doubleBytes >= 1024.0 && suffixIndex < 4) {
        doubleBytes /= 1024.0;
        suffixIndex++;
    }

    char buffer[64];
    if (suffixIndex == 0) {
        snprintf(buffer, sizeof(buffer), "%d %s", static_cast<int>(doubleBytes), suffixes[suffixIndex]);
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f %s", doubleBytes, suffixes[suffixIndex]);
    }
    return std::string(buffer);
}

} // namespace pserv
