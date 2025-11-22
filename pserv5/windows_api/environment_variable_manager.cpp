#include "precomp.h"
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/environment_variable_manager.h>
#include <core/data_object_container.h>

namespace pserv
{

    void EnvironmentVariableManager::EnumerateEnvironmentVariables(DataObjectContainer* doc)
    {
        // Enumerate system environment variables
        EnumerateFromKey(HKEY_LOCAL_MACHINE,
            L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
            EnvironmentVariableScope::System, doc);

        // Enumerate user environment variables
        EnumerateFromKey(HKEY_CURRENT_USER,
            L"Environment",
            EnvironmentVariableScope::User, doc);
    }

    void EnvironmentVariableManager::EnumerateFromKey(HKEY hKeyRoot, const std::wstring &subKeyPath, EnvironmentVariableScope scope, DataObjectContainer *doc)
    {
        wil::unique_hkey hKey;
        LSTATUS status = RegOpenKeyExW(hKeyRoot, subKeyPath.c_str(), 0, KEY_READ, &hKey);
        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegOpenKeyExW", status, "opening environment key '{}' for enumeration", utils::WideToUtf8(subKeyPath));
            return;
        }

        DWORD index = 0;
        wchar_t valueName[16384]; // Max value name length
        DWORD valueNameSize;
        DWORD valueType;
        BYTE valueData[32768]; // Max value data size
        DWORD valueDataSize;

        while (true)
        {
            valueNameSize = sizeof(valueName) / sizeof(wchar_t);
            valueDataSize = sizeof(valueData);

            status = RegEnumValueW(hKey.get(), index++, valueName, &valueNameSize, nullptr, &valueType, valueData, &valueDataSize);

            if (status == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegEnumValueW", status, "enumerating environment variables");
                break;
            }

            // Only process string types (REG_SZ and REG_EXPAND_SZ)
            if (valueType != REG_SZ && valueType != REG_EXPAND_SZ)
            {
                continue;
            }

            // Convert to UTF-8
            std::string name = utils::WideToUtf8(valueName);
            std::string value = utils::WideToUtf8(reinterpret_cast<wchar_t *>(valueData));

            const auto stableId{EnvironmentVariableInfo::GetStableID(scope, name)};
            auto info = doc->GetByStableId<EnvironmentVariableInfo>(stableId);
            if (info == nullptr)
            {
                info = doc->Append<EnvironmentVariableInfo>(DBG_NEW EnvironmentVariableInfo{name, value, scope});
            }
            else
            {
                info->SetValue(value);
            }
        }
    }

    bool EnvironmentVariableManager::SetEnvironmentVariable(const std::string &name, const std::string &value, EnvironmentVariableScope scope)
    {
        wil::unique_hkey hKey;
        LSTATUS status = RegOpenKeyExW(GetRegistryRoot(scope), GetRegistryPath(scope).c_str(), 0, KEY_SET_VALUE, &hKey);
        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegOpenKeyExW", status, "opening environment key for scope {} to set '{}'",
                scope == EnvironmentVariableScope::System ? "System" : "User", name);
            return false;
        }

        std::wstring wName = utils::Utf8ToWide(name);
        std::wstring wValue = utils::Utf8ToWide(value);

        status = RegSetValueExW(hKey.get(), wName.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE *>(wValue.c_str()),
            static_cast<DWORD>((wValue.length() + 1) * sizeof(wchar_t)));

        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegSetValueExW", status, "setting environment variable '{}'", name);
            return false;
        }

        BroadcastEnvironmentChange();
        spdlog::info("Set environment variable '{}' = '{}' (scope: {})", name, value,
            scope == EnvironmentVariableScope::System ? "System" : "User");
        return true;
    }

    bool EnvironmentVariableManager::DeleteEnvironmentVariable(const std::string &name, EnvironmentVariableScope scope)
    {
        wil::unique_hkey hKey;
        LSTATUS status = RegOpenKeyExW(GetRegistryRoot(scope), GetRegistryPath(scope).c_str(), 0, KEY_SET_VALUE, &hKey);
        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegOpenKeyExW", status, "opening environment key for scope {} to delete '{}'",
                scope == EnvironmentVariableScope::System ? "System" : "User", name);
            return false;
        }

        std::wstring wName = utils::Utf8ToWide(name);
        status = RegDeleteValueW(hKey.get(), wName.c_str());

        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegDeleteValueW", status, "deleting environment variable '{}'", name);
            return false;
        }

        BroadcastEnvironmentChange();
        spdlog::info("Deleted environment variable '{}' (scope: {})", name,
            scope == EnvironmentVariableScope::System ? "System" : "User");
        return true;
    }

    std::wstring EnvironmentVariableManager::GetRegistryPath(EnvironmentVariableScope scope)
    {
        switch (scope)
        {
        case EnvironmentVariableScope::System:
            return L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
        case EnvironmentVariableScope::User:
            return L"Environment";
        default:
            return L"";
        }
    }

    HKEY EnvironmentVariableManager::GetRegistryRoot(EnvironmentVariableScope scope)
    {
        switch (scope)
        {
        case EnvironmentVariableScope::System:
            return HKEY_LOCAL_MACHINE;
        case EnvironmentVariableScope::User:
            return HKEY_CURRENT_USER;
        default:
            return nullptr;
        }
    }

    void EnvironmentVariableManager::BroadcastEnvironmentChange()
    {
        // Notify all windows that environment has changed
        DWORD_PTR result;
        LRESULT sendResult = SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
            reinterpret_cast<LPARAM>(L"Environment"),
            SMTO_ABORTIFHUNG, 5000, &result);

        if (sendResult == 0)
        {
            LogWin32Error("SendMessageTimeoutW", "broadcasting WM_SETTINGCHANGE for environment");
        }
    }

} // namespace pserv
