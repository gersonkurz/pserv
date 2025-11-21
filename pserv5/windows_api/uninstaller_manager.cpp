#include "precomp.h"
#include <windows_api/uninstaller_manager.h>
#include <utils/string_utils.h>
#include <utils/format_utils.h>
#include <utils/win32_error.h>

namespace
{
    std::string GetInstalledProgramInfoGetId(const pserv::InstalledProgramInfo* program) {
        // A combination of DisplayName and UninstallString should be unique enough
        return std::format("{}_{}", program->GetDisplayName(), program->GetUninstallString());
    }

}

namespace pserv {




std::vector<DataObject*> UninstallerManager::EnumerateInstalledPrograms() {
    std::vector<DataObject*> programs;
    std::set<std::string> uniqueIds; // To store unique IDs and prevent duplicates

    // Enumerate from HKEY_LOCAL_MACHINE (64-bit and 32-bit uninstall paths)
    EnumerateProgramsInKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", programs);
    EnumerateProgramsInKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", programs);

    // Enumerate from HKEY_CURRENT_USER
    EnumerateProgramsInKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", programs);

    // Filter out duplicates. Programs can appear in multiple locations (e.g., user-specific vs. machine-wide)
    std::vector<DataObject*> uniquePrograms;
    for (const auto program : programs) {
        const auto id = GetInstalledProgramInfoGetId(static_cast<InstalledProgramInfo*>(program));
        if (uniqueIds.find(id) == uniqueIds.end()) {
            uniqueIds.insert(id);
            uniquePrograms.push_back(program);
        } else {
            // Duplicate, delete it
            delete program;
        }
    }

    return uniquePrograms;
}

void UninstallerManager::EnumerateProgramsInKey(
    HKEY hKeyParent,
    const std::wstring& subKeyPath,
    std::vector<DataObject*>& programs) {

    wil::unique_hkey hKey;
    LSTATUS status = RegOpenKeyExW(hKeyParent, subKeyPath.c_str(), 0, KEY_READ, &hKey);
    if (status != ERROR_SUCCESS) {
        spdlog::trace("Failed to open registry key {}: {}", pserv::utils::WideToUtf8(subKeyPath), pserv::utils::GetWin32ErrorMessage(status));
        return;
    }

    DWORD i = 0;
    wchar_t subKeyName[256];
    DWORD subKeyNameSize;

    while (true) {
        subKeyNameSize = sizeof(subKeyName) / sizeof(wchar_t);
        status = RegEnumKeyExW(hKey.get(), i++, subKeyName, &subKeyNameSize, nullptr, nullptr, nullptr, nullptr);

        if (status == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (status != ERROR_SUCCESS) {
            spdlog::warn("Failed to enumerate registry subkeys under {}: {}", pserv::utils::WideToUtf8(subKeyPath), pserv::utils::GetWin32ErrorMessage(status));
            break;
        }

        wil::unique_hkey hSubKey;
        status = RegOpenKeyExW(hKey.get(), subKeyName, 0, KEY_READ, &hSubKey);
        if (status != ERROR_SUCCESS) {
            spdlog::warn("Failed to open registry subkey {}: {}", pserv::utils::WideToUtf8(subKeyName), pserv::utils::GetWin32ErrorMessage(status));
            continue;
        }

        std::string displayName = GetRegistryStringValue(hSubKey.get(), L"DisplayName");
        if (displayName.empty()) {
            // Must have a display name to be considered a valid program entry
            continue;
        }

        // Get estimated size in KB (registry stores as DWORD)
        DWORD sizeKB = GetRegistryDwordValue(hSubKey.get(), L"EstimatedSize");
        uint64_t sizeBytes = static_cast<uint64_t>(sizeKB) * 1024;

        // Format size as string for display
        std::string sizeStr = utils::FormatSize(sizeBytes);

        programs.push_back(new InstalledProgramInfo(
            displayName,
            GetRegistryStringValue(hSubKey.get(), L"DisplayVersion"),
            GetRegistryStringValue(hSubKey.get(), L"Publisher"),
            GetRegistryStringValue(hSubKey.get(), L"InstallLocation"),
            GetRegistryStringValue(hSubKey.get(), L"UninstallString"),
            GetRegistryStringValue(hSubKey.get(), L"InstallDate"),
            sizeStr,
            GetRegistryStringValue(hSubKey.get(), L"Comments"),
            GetRegistryStringValue(hSubKey.get(), L"HelpLink"),
            GetRegistryStringValue(hSubKey.get(), L"URLInfoAbout"),
            sizeBytes
        ));
    }
}

std::string UninstallerManager::GetRegistryStringValue(
    HKEY hKey,
    const std::wstring& valueName) {

    DWORD type;
    DWORD dataSize;

    // Query size first
    LSTATUS status = RegQueryValueExW(hKey, valueName.c_str(), nullptr, &type, nullptr, &dataSize);
    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        return "";
    }

    std::vector<wchar_t> data(dataSize / sizeof(wchar_t));
    status = RegQueryValueExW(hKey, valueName.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(data.data()), &dataSize);
    if (status != ERROR_SUCCESS) {
        return "";
    }

    // Ensure null termination and convert to UTF-8
    data[dataSize / sizeof(wchar_t) - 1] = L'\0';
    return pserv::utils::WideToUtf8(data.data());
}

DWORD UninstallerManager::GetRegistryDwordValue(
    HKEY hKey,
    const std::wstring& valueName) {

    DWORD type;
    DWORD value = 0;
    DWORD dataSize = sizeof(DWORD);

    LSTATUS status = RegQueryValueExW(hKey, valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(&value), &dataSize);
    if (status != ERROR_SUCCESS || type != REG_DWORD) {
        return 0;
    }

    return value;
}

} // namespace pserv