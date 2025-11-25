/// @file process_manager.h
/// @brief Windows process enumeration and management API wrapper.
///
/// Provides process listing via Tool Help API and process control
/// operations like termination and priority changes.
#pragma once

namespace pserv
{
    class DataObjectContainer;

    /// @brief Namespace for process management functions.
    ///
    /// Uses CreateToolhelp32Snapshot for enumeration and OpenProcess
    /// for control operations.
    namespace ProcessManager
    {
        /// @brief Enumerate all running processes into a container.
        /// @param doc Container to populate with ProcessInfo objects.
        void EnumerateProcesses(DataObjectContainer *doc);

        /// @brief Terminate a process by its ID.
        /// @param pid Process ID to terminate.
        /// @return true on success, false on failure.
        bool TerminateProcessById(DWORD pid);

        /// @brief Change a process's priority class.
        /// @param pid Process ID to modify.
        /// @param priorityClass IDLE_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, etc.
        /// @return true on success, false on failure.
        bool SetProcessPriority(DWORD pid, DWORD priorityClass);

        /// @brief Get the executable path for a process.
        /// @param pid Process ID to query.
        /// @return Full path to executable, or empty string on failure.
        std::string GetProcessPath(DWORD pid);
    };

} // namespace pserv
