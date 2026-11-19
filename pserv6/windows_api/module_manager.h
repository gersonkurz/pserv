/// @file module_manager.h
/// @brief Windows module (DLL) enumeration API wrapper.
///
/// Provides module listing for a specific process using the
/// PSAPI EnumProcessModules function.
#pragma once

namespace pserv
{
    class DataObjectContainer;

    /// @brief Static class for module enumeration.
    ///
    /// Uses EnumProcessModules from PSAPI to list all DLLs loaded
    /// in a target process's address space.
    class ModuleManager final
    {
    public:
        /// @brief Enumerate all modules loaded in a process.
        /// @param doc Container to populate with ModuleInfo objects.
        /// @param processId Target process ID.
        static void EnumerateModules(DataObjectContainer *doc, uint32_t processId);

    private:
        static std::string RetrieveModuleBaseName(HANDLE hProcess, HMODULE hModule);
        static std::string RetrieveModuleFileName(HANDLE hProcess, HMODULE hModule);
    };

} // namespace pserv
