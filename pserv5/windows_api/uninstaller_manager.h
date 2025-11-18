#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "../models/installed_program_info.h"
#include <wil/resource.h>
#include <winreg.h>

namespace pserv {

class UninstallerManager {
public:
    // Enumerates all installed programs from various registry locations.
    // Returns raw pointers - caller is responsible for cleanup.
    static std::vector<InstalledProgramInfo*> EnumerateInstalledPrograms();

private:
    // Helper to read program info from a specific registry key.
    static void EnumerateProgramsInKey(
        HKEY hKeyParent,
        const std::wstring& subKeyPath,
        std::vector<InstalledProgramInfo*>& programs);

    // Helper to read a string value from a registry key.
    static std::string GetRegistryStringValue(
        HKEY hKey,
        const std::wstring& valueName);
};

} // namespace pserv
