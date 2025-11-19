#pragma once
#include "../core/data_object.h"
#include <Windows.h>
#include <string>

namespace pserv {

enum class ProcessProperty {
    Name = 0,
    PID,
    User,
    Priority,
    ThreadCount,
    WorkingSetSize,
    PrivatePageCount,
    Path,
    CommandLine,
    // Extras (not shown in default columns or hidden)
    ParentPID,
    PeakWorkingSetSize,
    VirtualSize,
    HandleCount,
    SessionId,
    // New columns
    StartTime,
    TotalCPUTime,
    UserCPUTime,
    KernelCPUTime,
    PagedPoolUsage,
    NonPagedPoolUsage,
    PageFaultCount
};

class ProcessInfo : public DataObject {
private:
    DWORD m_pid{};
    DWORD m_parentPid{};
    std::string m_name{};
    DWORD m_threadCount{};
    DWORD m_priorityClass{};
    std::string m_user{};
    std::string m_path{};
    std::string m_commandLine{};
    SIZE_T m_workingSetSize{};
    SIZE_T m_peakWorkingSetSize{};
    SIZE_T m_privatePageCount{};
    SIZE_T m_virtualSize{};
    DWORD m_handleCount{};
    DWORD m_sessionId{};
    
    // New fields
    FILETIME m_creationTime{};
    FILETIME m_exitTime{}; // Unused for running processes
    FILETIME m_kernelTime{};
    FILETIME m_userTime{};
    SIZE_T m_quotaPagedPoolUsage{};
    SIZE_T m_quotaNonPagedPoolUsage{};
    DWORD m_pageFaultCount{};

public:
    ProcessInfo(DWORD pid, std::string name);
    ~ProcessInfo() override = default;

    // DataObject interface
    std::string GetProperty(int propertyId) const override;
    PropertyValue GetTypedProperty(int propertyId) const override;
    bool MatchesFilter(const std::string& filter) const override;

    // Getters
    DWORD GetPid() const { return m_pid; }
    DWORD GetParentPid() const { return m_parentPid; }
    const std::string& GetName() const { return m_name; }
    DWORD GetThreadCount() const { return m_threadCount; }
    DWORD GetPriorityClass() const { return m_priorityClass; }
    const std::string& GetUser() const { return m_user; }
    const std::string& GetPath() const { return m_path; }
    const std::string& GetCommandLine() const { return m_commandLine; }
    SIZE_T GetWorkingSetSize() const { return m_workingSetSize; }
    SIZE_T GetPeakWorkingSetSize() const { return m_peakWorkingSetSize; }
    SIZE_T GetPrivatePageCount() const { return m_privatePageCount; }
    SIZE_T GetVirtualSize() const { return m_virtualSize; }
    DWORD GetHandleCount() const { return m_handleCount; }
    DWORD GetSessionId() const { return m_sessionId; }
    DWORD GetPageFaultCount() const { return m_pageFaultCount; }
    SIZE_T GetPagedPoolUsage() const { return m_quotaPagedPoolUsage; }
    SIZE_T GetNonPagedPoolUsage() const { return m_quotaNonPagedPoolUsage; }

    // Setters
    void SetParentPid(DWORD pid) { m_parentPid = pid; }
    void SetThreadCount(DWORD count) { m_threadCount = count; }
    void SetPriorityClass(DWORD priority) { m_priorityClass = priority; }
    void SetUser(const std::string& user) { m_user = user; }
    void SetPath(const std::string& path) { m_path = path; }
    void SetCommandLine(const std::string& cmdLine) { m_commandLine = cmdLine; }
    void SetWorkingSetSize(SIZE_T size) { m_workingSetSize = size; }
    void SetPeakWorkingSetSize(SIZE_T size) { m_peakWorkingSetSize = size; }
    void SetPrivatePageCount(SIZE_T count) { m_privatePageCount = count; }
    void SetVirtualSize(SIZE_T size) { m_virtualSize = size; }
    void SetHandleCount(DWORD count) { m_handleCount = count; }
    void SetSessionId(DWORD sessionId) { m_sessionId = sessionId; }
    
    // New Setters
    void SetTimes(const FILETIME& creation, const FILETIME& exit, const FILETIME& kernel, const FILETIME& user) {
        m_creationTime = creation;
        m_exitTime = exit;
        m_kernelTime = kernel;
        m_userTime = user;
    }
    void SetMemoryExtras(SIZE_T pagedPool, SIZE_T nonPagedPool, DWORD pageFaults) {
        m_quotaPagedPoolUsage = pagedPool;
        m_quotaNonPagedPoolUsage = nonPagedPool;
        m_pageFaultCount = pageFaults;
    }

    // Helpers
    std::string GetPriorityString() const;
    static std::string FileTimeToString(const FILETIME& ft);
    static std::string DurationToString(const FILETIME& ft); // Treat FILETIME as duration
};

} // namespace pserv
