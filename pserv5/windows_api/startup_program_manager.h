#pragma once
#include <core/data_object.h>
#include <models/startup_program_info.h>

namespace pserv
{
    class DataObjectContainer;

    class StartupProgramManager final
    {
    public:
        // Enumerate all startup programs
        static void EnumerateStartupPrograms(DataObjectContainer *doc);

        // Enable/disable registry startup items (by renaming registry value)
        static bool SetEnabled(StartupProgramInfo *program, bool enabled);

        // Delete startup item
        static bool DeleteStartupProgram(StartupProgramInfo *program);

    private:
        // Helper to enumerate from registry Run keys
        static void EnumerateRegistryRun(HKEY hKeyRoot,
            const std::wstring &subKeyPath,
            StartupProgramScope scope,
            StartupProgramType type,
            const std::string &locationDesc,
            DataObjectContainer *doc);

        // Helper to enumerate from startup folders
        static void EnumerateStartupFolder(
            const std::wstring &folderPath, StartupProgramScope scope, const std::string &locationDesc, DataObjectContainer *doc);

        // Helper to get special folder paths
        static std::wstring GetCommonStartupFolder();
        static std::wstring GetUserStartupFolder();

        // Helper to resolve .lnk shortcuts
        static std::wstring ResolveLnkTarget(const std::wstring &lnkPath);
    };

} // namespace pserv
