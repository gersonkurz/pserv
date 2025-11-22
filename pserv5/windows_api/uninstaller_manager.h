#pragma once


namespace pserv
{
    class DataObjectContainer;

    class UninstallerManager final
    {
    public:
        // Enumerates all installed programs from various registry locations.
        // Returns raw pointers - caller is responsible for cleanup.
        static void EnumerateInstalledPrograms(DataObjectContainer *doc);

    private:
        UninstallerManager() = delete;

        // Helper to read program info from a specific registry key.
        static void EnumerateProgramsInKey(HKEY hKeyParent, const std::wstring &subKeyPath, DataObjectContainer* doc);

        // Helper to read a string value from a registry key.
        static std::string GetRegistryStringValue(HKEY hKey, const std::wstring &valueName);

        // Helper to read a DWORD value from a registry key.
        static DWORD GetRegistryDwordValue(HKEY hKey, const std::wstring &valueName);
    };

} // namespace pserv
