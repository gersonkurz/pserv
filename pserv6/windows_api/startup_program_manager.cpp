#include "precomp.h"
#include <utils/com_scope.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/startup_program_manager.h>
#include <core/data_object_container.h>
#include <shlobj.h>
#include <filesystem>

namespace pserv
{

    void StartupProgramManager::EnumerateStartupPrograms(DataObjectContainer *doc)
    {
        // Enumerate registry Run keys (HKLM - System scope)
        EnumerateRegistryRun(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            StartupProgramScope::System,
            StartupProgramType::RegistryRun,
            "HKLM Run",
            doc);

        EnumerateRegistryRun(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
            StartupProgramScope::System,
            StartupProgramType::RegistryRunOnce,
            "HKLM RunOnce",
            doc);

        // 32-bit registry on 64-bit Windows
        EnumerateRegistryRun(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Run",
            StartupProgramScope::System,
            StartupProgramType::RegistryRun,
            "HKLM Run (32-bit)",
            doc);

        // Enumerate registry Run keys (HKCU - User scope)
        EnumerateRegistryRun(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            StartupProgramScope::User,
            StartupProgramType::RegistryRun,
            "HKCU Run",
            doc);

        EnumerateRegistryRun(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
            StartupProgramScope::User,
            StartupProgramType::RegistryRunOnce,
            "HKCU RunOnce",
            doc);

        // Enumerate startup folders
        std::wstring commonStartup = GetCommonStartupFolder();
        if (!commonStartup.empty())
        {
            EnumerateStartupFolder(commonStartup, StartupProgramScope::System, "Common Startup Folder", doc);
        }

        std::wstring userStartup = GetUserStartupFolder();
        if (!userStartup.empty())
        {
            EnumerateStartupFolder(userStartup, StartupProgramScope::User, "User Startup Folder", doc);
        }
    }

    void StartupProgramManager::EnumerateRegistryRun(HKEY hKeyRoot,
        const std::wstring &subKeyPath,
        StartupProgramScope scope,
        StartupProgramType type,
        const std::string &locationDesc,
        DataObjectContainer *doc)
    {
        wil::unique_hkey hKey;
        LSTATUS status = RegOpenKeyExW(hKeyRoot, subKeyPath.c_str(), 0, KEY_READ, &hKey);
        if (status != ERROR_SUCCESS)
        {
            // Not all Run keys exist on all systems, this is expected
            LogExpectedWin32ErrorCode("RegOpenKeyExW", status, "opening startup key '{}'", utils::WideToUtf8(subKeyPath));
            return;
        }

        DWORD index = 0;
        wchar_t valueName[16384];
        DWORD valueNameSize;
        DWORD valueType;
        BYTE valueData[32768];
        DWORD valueDataSize;

        std::vector<std::pair<std::string, std::string>> values; // value name, command
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
                LogWin32ErrorCode("RegEnumValueW", status, "enumerating startup registry values in '{}'", locationDesc);
                break;
            }

            // Only process string types
            if (valueType != REG_SZ && valueType != REG_EXPAND_SZ)
            {
                continue;
            }
            values.emplace_back(utils::WideToUtf8(valueName), utils::WideToUtf8(reinterpret_cast<wchar_t *>(valueData)));
        }

        // SetEnabled(false) renames a value "X" to "Disabled_X". Such a value is listed as X,
        // disabled, under X's identity, so it can be enabled again after a refresh (it used to show
        // up as an enabled "Disabled_X"). If "X" exists as well, the disabled one keeps its own name.
        constexpr std::string_view disabledPrefix{"Disabled_"};
        std::set<std::string> names;
        for (const auto &[name, command] : values)
            names.insert(name);
        for (const auto &[valueNameUtf8, command] : values)
        {
            std::string name = valueNameUtf8;
            bool bEnabled = true;
            if (name.starts_with(disabledPrefix) && name.size() > disabledPrefix.size())
            {
                bEnabled = false;
                if (!names.contains(name.substr(disabledPrefix.size())))
                    name = name.substr(disabledPrefix.size());
            }

            const auto stableId{StartupProgramInfo::GetStableID(name, type, scope)};
            auto program = doc->GetByStableId<StartupProgramInfo>(stableId);
            if (program == nullptr)
            {
                program = doc->Append<StartupProgramInfo>(DBG_NEW StartupProgramInfo{name, command, locationDesc, type, scope, bEnabled});
            }
            program->SetEnabled(bEnabled);
            program->SetRegistryPath(utils::WideToUtf8(subKeyPath));
            program->SetRegistryValueName(valueNameUtf8);
        }
    }

    void StartupProgramManager::EnumerateStartupFolder(const std::wstring &folderPath,
        StartupProgramScope scope,
        const std::string &locationDesc, DataObjectContainer *doc)
    {
        try
        {
            if (!std::filesystem::exists(folderPath))
            {
                return;
            }

            for (const auto &entry : std::filesystem::directory_iterator(folderPath))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                std::wstring filePath = entry.path().wstring();
                std::string fileName = utils::WideToUtf8(entry.path().filename().wstring());

                // Resolve .lnk shortcuts
                std::wstring targetPath = filePath;
                if (entry.path().extension() == L".lnk")
                {
                    targetPath = ResolveLnkTarget(filePath);
                    if (targetPath.empty())
                    {
                        targetPath = filePath; // Fall back to .lnk path if resolution fails
                    }
                }

                std::string command = utils::WideToUtf8(targetPath);
                const auto type = StartupProgramType::StartupFolder;
                const auto stableId{StartupProgramInfo::GetStableID(fileName, type, scope)};
                auto program = doc->GetByStableId<StartupProgramInfo>(stableId);
                if (program == nullptr)
                {
                    program = doc->Append<StartupProgramInfo>(DBG_NEW StartupProgramInfo{fileName, command, locationDesc, type, scope, true});
                }
                program->SetFilePath(utils::WideToUtf8(filePath));
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            spdlog::error("Failed to enumerate startup folder '{}': {}", utils::WideToUtf8(folderPath), e.what());
        }
    }

    bool StartupProgramManager::SetEnabled(StartupProgramInfo *program, bool enabled)
    {
        // Only registry items can be enabled/disabled
        if (program->GetType() == StartupProgramType::StartupFolder)
        {
            spdlog::warn("Cannot enable/disable startup folder items via registry");
            return false;
        }

        // For registry items, we disable by prefixing the value name with a marker
        // This is a common pattern used by tools like msconfig
        std::string valueName = program->GetRegistryValueName();
        std::string registryPath = program->GetRegistryPath();

        // Determine registry root
        HKEY hKeyRoot = program->GetScope() == StartupProgramScope::System ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

        wil::unique_hkey hKey;
        LSTATUS status = RegOpenKeyExW(hKeyRoot, utils::Utf8ToWide(registryPath).c_str(), 0, KEY_READ | KEY_WRITE, &hKey);
        if (status != ERROR_SUCCESS)
        {
            LogWin32ErrorCode("RegOpenKeyExW", status, "opening startup registry key for enable/disable");
            return false;
        }

        if (!enabled && program->IsEnabled())
        {
            // Disable: Rename value by prefixing with "Disabled_"
            std::wstring wOldName = utils::Utf8ToWide(valueName);
            std::wstring wNewName = L"Disabled_" + wOldName;

            // Never overwrite another value (registry value names compare case-insensitively,
            // and RegQueryValueExW does too)
            if (RegQueryValueExW(hKey.get(), wNewName.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
            {
                spdlog::error("Cannot disable '{}': a value '{}' already exists", valueName, utils::WideToUtf8(wNewName));
                return false;
            }

            // Read current value
            DWORD valueType;
            BYTE valueData[32768];
            DWORD valueDataSize = sizeof(valueData);

            status = RegQueryValueExW(hKey.get(), wOldName.c_str(), nullptr, &valueType, valueData, &valueDataSize);
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegQueryValueExW", status, "reading startup value '{}'", valueName);
                return false;
            }

            // Write with new name
            status = RegSetValueExW(hKey.get(), wNewName.c_str(), 0, valueType, valueData, valueDataSize);
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegSetValueExW", status, "writing disabled startup value");
                return false;
            }

            // Delete old name
            status = RegDeleteValueW(hKey.get(), wOldName.c_str());
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegDeleteValueW", status, "deleting original startup value");
                return false;
            }

            program->SetEnabled(false);
            program->SetRegistryValueName(utils::WideToUtf8(wNewName));
            spdlog::info("Disabled startup program: {}", program->GetName());
            return true;
        }
        else if (enabled && !program->IsEnabled())
        {
            // Enable: Remove "Disabled_" prefix
            if (valueName.starts_with("Disabled_"))
            {
                std::string newName = valueName.substr(9); // Remove "Disabled_" prefix
                std::wstring wOldName = utils::Utf8ToWide(valueName);
                std::wstring wNewName = utils::Utf8ToWide(newName);

                // Never overwrite the enabled twin "X" of "Disabled_X"
                if (RegQueryValueExW(hKey.get(), wNewName.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
                {
                    spdlog::error("Cannot enable '{}': a value '{}' already exists", valueName, newName);
                    return false;
                }

                // Read current value
                DWORD valueType;
                BYTE valueData[32768];
                DWORD valueDataSize = sizeof(valueData);

                status = RegQueryValueExW(hKey.get(), wOldName.c_str(), nullptr, &valueType, valueData, &valueDataSize);
                if (status != ERROR_SUCCESS)
                {
                    LogWin32ErrorCode("RegQueryValueExW", status, "reading disabled startup value");
                    return false;
                }

                // Write with new name
                status = RegSetValueExW(hKey.get(), wNewName.c_str(), 0, valueType, valueData, valueDataSize);
                if (status != ERROR_SUCCESS)
                {
                    LogWin32ErrorCode("RegSetValueExW", status, "writing enabled startup value");
                    return false;
                }

                // Delete old name
                status = RegDeleteValueW(hKey.get(), wOldName.c_str());
                if (status != ERROR_SUCCESS)
                {
                    LogWin32ErrorCode("RegDeleteValueW", status, "deleting disabled startup value");
                    return false;
                }

                program->SetEnabled(true);
                program->SetRegistryValueName(newName);
                spdlog::info("Enabled startup program: {}", program->GetName());
                return true;
            }
        }

        return false;
    }

    bool StartupProgramManager::DeleteStartupProgram(StartupProgramInfo *program)
    {
        if (program->GetType() == StartupProgramType::StartupFolder)
        {
            // Delete file
            std::wstring filePath = utils::Utf8ToWide(program->GetFilePath());
            if (!DeleteFileW(filePath.c_str()))
            {
                LogWin32Error("DeleteFileW", "deleting startup file '{}'", program->GetFilePath());
                return false;
            }

            spdlog::info("Deleted startup program file: {}", program->GetName());
            return true;
        }
        else
        {
            // Delete registry value
            HKEY hKeyRoot = program->GetScope() == StartupProgramScope::System ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
            std::wstring registryPath = utils::Utf8ToWide(program->GetRegistryPath());
            std::wstring valueName = utils::Utf8ToWide(program->GetRegistryValueName());

            wil::unique_hkey hKey;
            LSTATUS status = RegOpenKeyExW(hKeyRoot, registryPath.c_str(), 0, KEY_SET_VALUE, &hKey);
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegOpenKeyExW", status, "opening startup registry key for deletion");
                return false;
            }

            status = RegDeleteValueW(hKey.get(), valueName.c_str());
            if (status != ERROR_SUCCESS)
            {
                LogWin32ErrorCode("RegDeleteValueW", status, "deleting startup registry value '{}'", program->GetRegistryValueName());
                return false;
            }

            spdlog::info("Deleted startup program registry value: {}", program->GetName());
            return true;
        }
    }

    std::wstring StartupProgramManager::GetCommonStartupFolder()
    {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_STARTUP, NULL, 0, path)))
        {
            return path;
        }
        return L"";
    }

    std::wstring StartupProgramManager::GetUserStartupFolder()
    {
        wchar_t path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, path)))
        {
            return path;
        }
        return L"";
    }

    std::wstring StartupProgramManager::ResolveLnkTarget(const std::wstring &lnkPath)
    {
        // COM for this scope (CoInitialize == apartment-threaded); declared before every COM
        // pointer, so they are released first. Proceeds even if COM was already initialized.
        utils::ComScope com{COINIT_APARTMENTTHREADED};
        HRESULT hr = S_OK;

        wil::com_ptr<IShellLinkW> psl;
        hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
        if (FAILED(hr))
        {
            LogWin32ErrorCode("CoCreateInstance", hr, "creating IShellLink for .lnk resolution");
            return L"";
        }

        wil::com_ptr<IPersistFile> ppf;
        hr = psl->QueryInterface(IID_PPV_ARGS(&ppf));
        if (FAILED(hr))
        {
            LogWin32ErrorCode("QueryInterface", hr, "getting IPersistFile for .lnk resolution");
            return L"";
        }

        hr = ppf->Load(lnkPath.c_str(), STGM_READ);
        if (FAILED(hr))
        {
            LogWin32ErrorCode("IPersistFile::Load", hr, "loading .lnk file '{}'", utils::WideToUtf8(lnkPath));
            return L"";
        }

        wchar_t targetPath[MAX_PATH];
        hr = psl->GetPath(targetPath, MAX_PATH, NULL, 0);
        if (FAILED(hr))
        {
            LogWin32ErrorCode("IShellLink::GetPath", hr, "resolving .lnk target");
            return L"";
        }


        return targetPath;
    }

} // namespace pserv
