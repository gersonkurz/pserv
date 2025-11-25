/// @file startup_program_manager.h
/// @brief Windows startup program enumeration and management.
///
/// Provides listing and control of programs configured to run at
/// Windows startup from registry Run keys and Startup folders.
#pragma once
#include <core/data_object.h>
#include <models/startup_program_info.h>

namespace pserv
{
    class DataObjectContainer;

    /// @brief Static class for startup program management.
    ///
    /// Enumerates startup entries from:
    /// - HKCU/HKLM Run and RunOnce registry keys
    /// - User and common Startup folders
    ///
    /// Supports enabling, disabling, and deleting startup entries.
    class StartupProgramManager final
    {
    public:
        /// @brief Enumerate all startup programs into a container.
        /// @param doc Container to populate with StartupProgramInfo objects.
        static void EnumerateStartupPrograms(DataObjectContainer *doc);

        /// @brief Enable or disable a startup entry.
        /// @param program The startup entry to modify.
        /// @param enabled true to enable, false to disable.
        /// @return true on success.
        /// @note Registry entries are disabled by renaming the value.
        static bool SetEnabled(StartupProgramInfo *program, bool enabled);

        /// @brief Delete a startup entry.
        /// @param program The startup entry to delete.
        /// @return true on success.
        static bool DeleteStartupProgram(StartupProgramInfo *program);

    private:
        static void EnumerateRegistryRun(HKEY hKeyRoot, const std::wstring &subKeyPath,
            StartupProgramScope scope, StartupProgramType type,
            const std::string &locationDesc, DataObjectContainer *doc);
        static void EnumerateStartupFolder(const std::wstring &folderPath,
            StartupProgramScope scope, const std::string &locationDesc, DataObjectContainer *doc);
        static std::wstring GetCommonStartupFolder();
        static std::wstring GetUserStartupFolder();
        static std::wstring ResolveLnkTarget(const std::wstring &lnkPath);
    };

} // namespace pserv
