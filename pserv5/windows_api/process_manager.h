#pragma once

namespace pserv
{
    class DataObjectContainer;

    namespace ProcessManager
    {
        // Enumerate all running processes
        // Returns raw pointers - caller is responsible for cleanup (typically via RefCount/release)
        void EnumerateProcesses(DataObjectContainer *doc);

        // Terminate a process by ID
        // Returns true on success
        bool TerminateProcessById(DWORD pid);

        // Set process priority
        // priorityClass: IDLE_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, etc.
        bool SetProcessPriority(DWORD pid, DWORD priorityClass);

        // Get full path for a process ID (helper)
        std::string GetProcessPath(DWORD pid);
    };

} // namespace pserv
