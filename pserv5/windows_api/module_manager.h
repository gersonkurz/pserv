#pragma once
#include <models/module_info.h>

namespace pserv
{

    class ModuleManager
    {
    public:
        // Enumerates all modules for a given process ID
        static std::vector<ModuleInfo *> EnumerateModules(uint32_t processId);

    private:
        // Helper to get module base name (e.g., "ntdll.dll")
        static std::string RetrieveModuleBaseName(HANDLE hProcess, HMODULE hModule);
        // Helper to get module full path (e.g., "C:\Windows\System32\ntdll.dll")
        static std::string RetrieveModuleFileName(HANDLE hProcess, HMODULE hModule);
    };

} // namespace pserv
