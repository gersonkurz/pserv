#include "precomp.h"
#include <windows_api/module_manager.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>

#pragma comment(lib, "psapi.lib")

namespace pserv {

// Cache structure for module information by file path
struct CachedModuleInfo {
    std::string name;      // Base name (e.g., "kernel32.dll")
    std::string path;      // Full path UTF-8 (e.g., "C:\Windows\System32\kernel32.dll")
    DWORD size;            // SizeOfImage
};

// Static cache for module information by wide path
static std::unordered_map<std::wstring, CachedModuleInfo> s_moduleCache;

std::string ModuleManager::RetrieveModuleBaseName(HANDLE hProcess, HMODULE hModule) {
    // NOTE: Use parentheses () not braces {} - braces create initializer_list with 2 chars!
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
    // NOTE: Use parentheses () not braces {} - braces create initializer_list with 2 chars!
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
        // First, get the full path (we need this as our cache key)
        // NOTE: Use parentheses () not braces {} - braces create initializer_list with 2 chars!
        std::wstring wPath(MAX_PATH, L'\0');
        DWORD pathResult = ::GetModuleFileNameExW(hProcess.get(), hModule, wPath.data(), static_cast<DWORD>(wPath.size()));
        if (pathResult == 0) {
            spdlog::warn("GetModuleFileNameExW failed for module {:#x} in process {}: {}",
                reinterpret_cast<uintptr_t>(hModule), processId, pserv::utils::GetLastWin32ErrorMessage());
            continue;
        }
        wPath.resize(pathResult);

        // Check cache
        auto cacheIt = s_moduleCache.find(wPath);
        if (cacheIt != s_moduleCache.end()) {
            // Cache hit! Use cached data, only need base address from this process
            MODULEINFO moduleInfo{};
            if (::GetModuleInformation(hProcess.get(), hModule, &moduleInfo, sizeof(moduleInfo))) {
                const CachedModuleInfo& cached = cacheIt->second;
                modules.push_back(new ModuleInfo(
                    processId,
                    moduleInfo.lpBaseOfDll,
                    cached.size,
                    cached.name,
                    cached.path
                ));
            }
        } else {
            // Cache miss - do full enumeration
            MODULEINFO moduleInfo{};
            if (::GetModuleInformation(hProcess.get(), hModule, &moduleInfo, sizeof(moduleInfo))) {
                const auto moduleName = RetrieveModuleBaseName(hProcess.get(), hModule);
                const auto modulePath = pserv::utils::WideToUtf8(wPath);

                // Only add if we got at least a name or path
                if (!moduleName.empty() || !modulePath.empty()) {
                    // Add to cache
                    CachedModuleInfo cached;
                    cached.name = moduleName;
                    cached.path = modulePath;
                    cached.size = moduleInfo.SizeOfImage;
                    s_moduleCache[wPath] = cached;

                    modules.push_back(new ModuleInfo{
                        processId,
                        moduleInfo.lpBaseOfDll,
                        moduleInfo.SizeOfImage,
                        moduleName,
                        modulePath
                        });
                }
            } else {
                spdlog::warn("GetModuleInformation failed for module {:#x} in process {}: {}",
                    reinterpret_cast<uintptr_t>(hModule), processId, pserv::utils::GetLastWin32ErrorMessage());
            }
        }
    }

    return modules;
}

} // namespace pserv
