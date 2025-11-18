#pragma once
#include <vector>
#include <string>
#include <functional>
#include <Windows.h>

namespace pserv {

class ProcessInfo; // Forward declaration

class ProcessManager {
public:
    ProcessManager() = default;
    ~ProcessManager() = default;

    // Enumerate all running processes
    // Returns raw pointers - caller is responsible for cleanup (typically via RefCount/release)
    static std::vector<ProcessInfo*> EnumerateProcesses();

    // Terminate a process by ID
    // Returns true on success
    static bool TerminateProcessById(DWORD pid);

    // Set process priority
    // priorityClass: IDLE_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, etc.
    static bool SetProcessPriority(DWORD pid, DWORD priorityClass);

    // Get full path for a process ID (helper)
    static std::string GetProcessPath(DWORD pid);
};

} // namespace pserv
