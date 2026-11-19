/// @file uninstaller_manager.h
/// @brief Windows installed programs registry API wrapper.
///
/// Provides enumeration of installed programs from the Windows
/// registry uninstall keys (both 32-bit and 64-bit locations).
#pragma once

namespace pserv
{
    class DataObjectContainer;

    /// @brief Static class for installed program enumeration.
    ///
    /// Reads from multiple registry locations:
    /// - HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall
    /// - HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall
    /// - HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall
    class UninstallerManager final
    {
    public:
        /// @brief Enumerate all installed programs into a container.
        /// @param doc Container to populate with InstalledProgramInfo objects.
        static void EnumerateInstalledPrograms(DataObjectContainer *doc);

    private:
        UninstallerManager() = delete;
        static void EnumerateProgramsInKey(HKEY hKeyParent, const std::wstring &subKeyPath, DataObjectContainer* doc);
        static std::string GetRegistryStringValue(HKEY hKey, const std::wstring &valueName);
        static DWORD GetRegistryDwordValue(HKEY hKey, const std::wstring &valueName);
    };

} // namespace pserv
