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
                info = doc->Append<EnvironmentVariableInfo>(DBG_NEW EnvironmentVariableInfo{name, value, scope, valueType});
            }
            else
            {
                info->SetValue(value);
                info->SetRegType(valueType);
            }
        }
    }

    bool EnvironmentVariableManager::IsValidVariableName(const std::string &name)
    {
        if (name.empty())
        {
            return false;
        }
        bool hasNonWhitespace = false;
        for (unsigned char ch : name)
        {
            if (ch == '=' || ch == '\0')
            {
                return false;
            }
            if (!std::isspace(ch))
            {
                hasNonWhitespace = true;
            }
        }
        return hasNonWhitespace;
    }

    bool EnvironmentVariableManager::VariableExists(const std::string &name, EnvironmentVariableScope scope)
    {
        if (!IsValidVariableName(name))
        {
            return false;
        }

        wil::unique_hkey hKey;
        LSTATUS status = RegOpenKeyExW(GetRegistryRoot(scope), GetRegistryPath(scope).c_str(), 0, KEY_QUERY_VALUE, &hKey);
        if (status != ERROR_SUCCESS)
        {
            return false;
        }

        std::wstring wName = utils::Utf8ToWide(name);
        DWORD type = 0;
        status = RegQueryValueExW(hKey.get(), wName.c_str(), nullptr, &type, nullptr, nullptr);
        return status == ERROR_SUCCESS;
    }

    bool EnvironmentVariableManager::SetEnvironmentVariable(
        const std::string &name, const std::string &value, EnvironmentVariableScope scope, DWORD regType)
    {
        if (!IsValidVariableName(name))
        {
            spdlog::error("Invalid environment variable name '{}'", name);
            return false;
        }

        wil::unique_hkey hKey;
        LSTATUS status = RegOpenKeyExW(GetRegistryRoot(scope), GetRegistryPath(scope).c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey);
        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegOpenKeyExW", status, "opening environment key for scope {} to set '{}'",
                scope == EnvironmentVariableScope::System ? "System" : "User", name);
            return false;
        }

        std::wstring wName = utils::Utf8ToWide(name);
        std::wstring wValue = utils::Utf8ToWide(value);

        DWORD writeType = regType;
        if (writeType != REG_SZ && writeType != REG_EXPAND_SZ)
        {
            DWORD existingType = 0;
            if (RegQueryValueExW(hKey.get(), wName.c_str(), nullptr, &existingType, nullptr, nullptr) == ERROR_SUCCESS &&
                (existingType == REG_SZ || existingType == REG_EXPAND_SZ))
            {
                writeType = existingType;
            }
            else
            {
                writeType = (value.find('%') != std::string::npos) ? REG_EXPAND_SZ : REG_SZ;
            }
        }

        status = RegSetValueExW(hKey.get(), wName.c_str(), 0, writeType,
            reinterpret_cast<const BYTE *>(wValue.c_str()),
            static_cast<DWORD>((wValue.length() + 1) * sizeof(wchar_t)));

        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegSetValueExW", status, "setting environment variable '{}'", name);
            return false;
        }

        BroadcastEnvironmentChange();
        spdlog::info("Set environment variable '{}' = '{}' (scope: {}, type: {})", name, value,
            scope == EnvironmentVariableScope::System ? "System" : "User",
            writeType == REG_EXPAND_SZ ? "REG_EXPAND_SZ" : "REG_SZ");
        return true;
    }

    bool EnvironmentVariableManager::RenameEnvironmentVariable(const std::string &oldName,
        const std::string &newName,
        const std::string &value,
        EnvironmentVariableScope scope,
        DWORD preferredRegType)
    {
        if (!IsValidVariableName(oldName) || !IsValidVariableName(newName))
        {
            spdlog::error("Invalid environment variable rename '{}' -> '{}'", oldName, newName);
            return false;
        }

        wil::unique_hkey hKey;
        LSTATUS status = RegOpenKeyExW(GetRegistryRoot(scope), GetRegistryPath(scope).c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey);
        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegOpenKeyExW", status, "opening environment key for scope {} to rename '{}' -> '{}'",
                scope == EnvironmentVariableScope::System ? "System" : "User", oldName, newName);
            return false;
        }

        std::wstring wOldName = utils::Utf8ToWide(oldName);
        std::wstring wNewName = utils::Utf8ToWide(newName);
        std::wstring wValue = utils::Utf8ToWide(value);

        // Preserve the existing registry value type (REG_SZ vs REG_EXPAND_SZ) and snapshot original bytes for rollback.
        DWORD existingType = 0;
        DWORD existingDataSize = 0;
        LSTATUS queryOldStatus = RegQueryValueExW(hKey.get(), wOldName.c_str(), nullptr, &existingType, nullptr, &existingDataSize);
        if (queryOldStatus != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegQueryValueExW", queryOldStatus, "querying existing environment variable '{}'", oldName);
            return false;
        }

        DWORD writeType = preferredRegType;
        if (writeType != REG_SZ && writeType != REG_EXPAND_SZ)
        {
            writeType = (existingType == REG_EXPAND_SZ || existingType == REG_SZ) ? existingType : REG_SZ;
        }

        bool isCaseOnlyRename = (_wcsicmp(wOldName.c_str(), wNewName.c_str()) == 0);
        if (!isCaseOnlyRename)
        {
            // Refuse to overwrite an existing variable with the same name (case-insensitive in registry).
            DWORD collisionType = 0;
            if (RegQueryValueExW(hKey.get(), wNewName.c_str(), nullptr, &collisionType, nullptr, nullptr) == ERROR_SUCCESS)
            {
                spdlog::error("Cannot rename environment variable '{}' to '{}': target variable already exists in scope {}",
                    oldName, newName, scope == EnvironmentVariableScope::System ? "System" : "User");
                return false;
            }

            // Write the new value FIRST so the original is never lost if writing fails.
            status = RegSetValueExW(hKey.get(), wNewName.c_str(), 0, writeType,
                reinterpret_cast<const BYTE *>(wValue.c_str()),
                static_cast<DWORD>((wValue.length() + 1) * sizeof(wchar_t)));
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegSetValueExW", status, "writing renamed environment variable '{}'", newName);
                return false;
            }

            // Delete the old value only after the new value is safely persisted.
            status = RegDeleteValueW(hKey.get(), wOldName.c_str());
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegDeleteValueW", status, "removing old environment variable '{}' after rename", oldName);
                // Roll back the new value so we do not leave duplicate entries behind.
                RegDeleteValueW(hKey.get(), wNewName.c_str());
                return false;
            }
        }
        else
        {
            // Case-only rename: read original data so we can roll back if re-creating with new casing fails.
            std::vector<BYTE> originalData(existingDataSize);
            if (existingDataSize > 0)
            {
                LSTATUS readStatus = RegQueryValueExW(hKey.get(), wOldName.c_str(), nullptr, &existingType, originalData.data(), &existingDataSize);
                if (readStatus != ERROR_SUCCESS)
                {
                    LogWin32ErrorCode("RegQueryValueExW", readStatus, "reading backup of environment variable '{}'", oldName);
                    return false;
                }
            }

            status = RegDeleteValueW(hKey.get(), wOldName.c_str());
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegDeleteValueW", status, "removing '{}' for case-only rename", oldName);
                return false;
            }

            status = RegSetValueExW(hKey.get(), wNewName.c_str(), 0, writeType,
                reinterpret_cast<const BYTE *>(wValue.c_str()),
                static_cast<DWORD>((wValue.length() + 1) * sizeof(wchar_t)));
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegSetValueExW", status, "writing case-renamed environment variable '{}'", newName);
                // Attempt rollback to original name & bytes.
                RegSetValueExW(hKey.get(), wOldName.c_str(), 0, existingType, originalData.data(), existingDataSize);
                return false;
            }
        }

        BroadcastEnvironmentChange();
        spdlog::info("Renamed environment variable '{}' -> '{}' (scope: {}, type: {})",
            oldName, newName, scope == EnvironmentVariableScope::System ? "System" : "User",
            writeType == REG_EXPAND_SZ ? "REG_EXPAND_SZ" : "REG_SZ");
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
