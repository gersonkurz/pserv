#include "precomp.h"
#include "module_manager.h"
#include <spdlog/spdlog.h>
#include <Psapi.h> // For EnumProcessModules, GetModuleBaseName, GetModuleFileNameEx, GetModuleInformation
#include <format>
#include "../utils/string_utils.h"
#include "../utils/win32_error.h"

#pragma comment(lib, "psapi.lib")

namespace pserv {

std::string ModuleManager::RetrieveModuleBaseName(HANDLE hProcess, HMODULE hModule) {
    std::wstring wName(MAX_PATH, L'\0');
    DWORD result = ::GetModuleBaseNameW(hProcess, hModule, wName.data(), static_cast<DWORD>(wName.size()));
    if (result == 0) {
        spdlog::warn("GetModuleBaseNameW failed for module {:#x}: {}", reinterpret_cast<uintptr_t>(hModule), pserv::utils::GetLastWin32ErrorMessage());
        return "";
    }
    wName.resize(result);
    return pserv::utils::WideToUtf8(wName);
}

std::string ModuleManager::RetrieveModuleFileName(HANDLE hProcess, HMODULE hModule) {
    std::wstring wPath(MAX_PATH, L'\0');
    DWORD result = ::GetModuleFileNameExW(hProcess, hModule, wPath.data(), static_cast<DWORD>(wPath.size()));
    if (result == 0) {
        spdlog::warn("GetModuleFileNameExW failed for module {:#x}: {}", reinterpret_cast<uintptr_t>(hModule), pserv::utils::GetLastWin32ErrorMessage());
        return "";
    }
    wPath.resize(result);
    return pserv::utils::WideToUtf8(wPath);
}

std::vector<ModuleInfo*> ModuleManager::EnumerateModules(uint32_t processId) {
    std::vector<ModuleInfo*> modules;

    wil::unique_handle hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId));
    if (!hProcess) {
        // Common for system processes or elevated processes. Just log as debug/trace.
        spdlog::trace("Failed to open process {}: {}", processId, pserv::utils::GetLastWin32ErrorMessage());
        return modules;
    }

    std::vector<HMODULE> hModules(1024); // Start with a reasonable buffer size
    DWORD cbNeeded;

    if (!::EnumProcessModules(hProcess.get(), hModules.data(), static_cast<DWORD>(hModules.size()) * sizeof(HMODULE), &cbNeeded)) {
        spdlog::warn("EnumProcessModules failed for process {}: {}", processId, pserv::utils::GetLastWin32ErrorMessage());
        return modules;
    }

    hModules.resize(cbNeeded / sizeof(HMODULE));

    for (HMODULE hModule : hModules) {
        MODULEINFO moduleInfo{};
        if (::GetModuleInformation(hProcess.get(), hModule, &moduleInfo, sizeof(moduleInfo))) {
            std::string moduleName = RetrieveModuleBaseName(hProcess.get(), hModule);
            std::string modulePath = RetrieveModuleFileName(hProcess.get(), hModule);

            // Only add if we got at least a name or path
            if (!moduleName.empty() || !modulePath.empty()) {
                modules.push_back(new ModuleInfo(
                    processId,
                    moduleInfo.lpBaseOfDll,
                    moduleInfo.SizeOfImage,
                    moduleName,
                    modulePath
                ));
            }
        } else {
            spdlog::warn("GetModuleInformation failed for module {:#x} in process {}: {}", reinterpret_cast<uintptr_t>(hModule), processId, pserv::utils::GetLastWin32ErrorMessage());
        }
    }

    return modules;
}

} // namespace pserv
