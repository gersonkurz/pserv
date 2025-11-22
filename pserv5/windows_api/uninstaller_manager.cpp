#include "precomp.h"
#include <utils/format_utils.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/uninstaller_manager.h>
#include <models/installed_program_info.h>
#include <core/data_object_container.h>


namespace pserv
{

    void UninstallerManager::EnumerateInstalledPrograms(DataObjectContainer* doc)
    {
        // Enumerate from HKEY_LOCAL_MACHINE (64-bit and 32-bit uninstall paths)
        EnumerateProgramsInKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", doc);
        EnumerateProgramsInKey(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall", doc);

        // Enumerate from HKEY_CURRENT_USER
        EnumerateProgramsInKey(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", doc);
    }

    void UninstallerManager::EnumerateProgramsInKey(HKEY hKeyParent, const std::wstring &subKeyPath, DataObjectContainer *doc)
    {
        wil::unique_hkey hKey;
        LSTATUS status = RegOpenKeyExW(hKeyParent, subKeyPath.c_str(), 0, KEY_READ, &hKey);
        if (status != ERROR_SUCCESS)
        {
            LogExpectedWin32ErrorCode("RegOpenKeyExW", status, "key '{}'", pserv::utils::WideToUtf8(subKeyPath));
            return;
        }

        DWORD i = 0;
        wchar_t subKeyName[256];
        DWORD subKeyNameSize;

        while (true)
        {
            subKeyNameSize = sizeof(subKeyName) / sizeof(wchar_t);
            status = RegEnumKeyExW(hKey.get(), i++, subKeyName, &subKeyNameSize, nullptr, nullptr, nullptr, nullptr);

            if (status == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegEnumKeyExW", status, "under key '{}'", pserv::utils::WideToUtf8(subKeyPath));
                break;
            }

            wil::unique_hkey hSubKey;
            status = RegOpenKeyExW(hKey.get(), subKeyName, 0, KEY_READ, &hSubKey);
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegOpenKeyExW", status, "subkey '{}'", pserv::utils::WideToUtf8(subKeyName));
                continue;
            }

            std::string displayName = GetRegistryStringValue(hSubKey.get(), L"DisplayName");
            if (displayName.empty())
            {
                // Must have a display name to be considered a valid program entry
                continue;
            }

            // Get estimated size in KB (registry stores as DWORD)
            DWORD sizeKB = GetRegistryDwordValue(hSubKey.get(), L"EstimatedSize");
            uint64_t sizeBytes = static_cast<uint64_t>(sizeKB) * 1024;

            // Format size as string for display
            std::string sizeStr = utils::FormatSize(sizeBytes);

            const auto displayVersion = GetRegistryStringValue(hSubKey.get(), L"DisplayVersion");
            const auto uninstallString = GetRegistryStringValue(hSubKey.get(), L"UninstallString");
            const auto stableId{InstalledProgramInfo::GetStableID(displayName, displayVersion, uninstallString)};
            auto ipi = doc->GetByStableId<InstalledProgramInfo>(stableId);
            if (ipi == nullptr)
            {
                ipi = doc->Append<InstalledProgramInfo>(DBG_NEW InstalledProgramInfo{displayName, displayVersion, uninstallString});
            }
            ipi->SetValues(
                GetRegistryStringValue(hSubKey.get(), L"Publisher"),
                GetRegistryStringValue(hSubKey.get(), L"InstallLocation"),
                GetRegistryStringValue(hSubKey.get(), L"InstallDate"),
                sizeStr,
                GetRegistryStringValue(hSubKey.get(), L"Comments"),
                GetRegistryStringValue(hSubKey.get(), L"HelpLink"),
                GetRegistryStringValue(hSubKey.get(), L"URLInfoAbout"),
                sizeBytes);
        }
    }

    std::string UninstallerManager::GetRegistryStringValue(HKEY hKey, const std::wstring &valueName)
    {

        DWORD type;
        DWORD dataSize;

        // Query size first
        LSTATUS status = RegQueryValueExW(hKey, valueName.c_str(), nullptr, &type, nullptr, &dataSize);
        if (status != ERROR_SUCCESS)
        {
            // Missing values are common/expected for optional registry entries
            if (status != ERROR_FILE_NOT_FOUND)
            {
                LogWin32ErrorCode("RegQueryValueExW", status, "value '{}'", pserv::utils::WideToUtf8(valueName));
            }
            return "";
        }

        if (type != REG_SZ && type != REG_EXPAND_SZ)
        {
            // Wrong type, not an error but not what we want
            return "";
        }

        std::vector<wchar_t> data(dataSize / sizeof(wchar_t));
        status = RegQueryValueExW(hKey, valueName.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(data.data()), &dataSize);
        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegQueryValueExW", status, "value '{}'", pserv::utils::WideToUtf8(valueName));
            return "";
        }

        // Ensure null termination and convert to UTF-8
        data[dataSize / sizeof(wchar_t) - 1] = L'\0';
        return pserv::utils::WideToUtf8(data.data());
    }

    DWORD UninstallerManager::GetRegistryDwordValue(HKEY hKey, const std::wstring &valueName)
    {

        DWORD type;
        DWORD value = 0;
        DWORD dataSize = sizeof(DWORD);

        LSTATUS status = RegQueryValueExW(hKey, valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(&value), &dataSize);
        if (status != ERROR_SUCCESS)
        {
            // Missing values are common/expected for optional registry entries
            if (status != ERROR_FILE_NOT_FOUND)
            {
                LogWin32ErrorCode("RegQueryValueExW", status, "DWORD value '{}'", pserv::utils::WideToUtf8(valueName));
            }
            return 0;
        }

        if (type != REG_DWORD)
        {
            // Wrong type, not an error but not what we want
            return 0;
        }

        return value;
    }

} // namespace pserv