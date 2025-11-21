#pragma once


namespace pserv
{
    class DataObject;

    class UninstallerManager final
    {
    public:
        // Enumerates all installed programs from various registry locations.
        // Returns raw pointers - caller is responsible for cleanup.
        static std::vector<DataObject *> EnumerateInstalledPrograms();

    private:
        UninstallerManager() = delete;

        // Helper to read program info from a specific registry key.
        static void EnumerateProgramsInKey(HKEY hKeyParent, const std::wstring &subKeyPath, std::vector<DataObject *> &programs);

        // Helper to read a string value from a registry key.
        static std::string GetRegistryStringValue(HKEY hKey, const std::wstring &valueName);

        // Helper to read a DWORD value from a registry key.
        static DWORD GetRegistryDwordValue(HKEY hKey, const std::wstring &valueName);
    };

} // namespace pserv
