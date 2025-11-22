#include "precomp.h"
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/module_manager.h>
#include <models/module_info.h>
#include <core/data_object_container.h>

#pragma comment(lib, "psapi.lib")

namespace pserv
{

    // Cache structure for module information by file path
    struct CachedModuleInfo
    {
        std::string name; // Base name (e.g., "kernel32.dll")
        std::string path; // Full path UTF-8 (e.g., "C:\Windows\System32\kernel32.dll")
        DWORD size;       // SizeOfImage
    };

    // Static cache for module information by wide path
    static std::unordered_map<std::wstring, CachedModuleInfo> s_moduleCache;

    std::string ModuleManager::RetrieveModuleBaseName(HANDLE hProcess, HMODULE hModule)
    {
        // NOTE: Use parentheses () not braces {} - braces create initializer_list with 2 chars!
        std::wstring wName(MAX_PATH, L'\0');
        DWORD result = ::GetModuleBaseNameW(hProcess, hModule, wName.data(), static_cast<DWORD>(wName.size()));
        if (result == 0)
        {
            LogWin32Error("GetModuleBaseNameW", "module {:#x}", reinterpret_cast<uintptr_t>(hModule));
            return "";
        }
        wName.resize(result);
        return pserv::utils::WideToUtf8(wName);
    }

    std::string ModuleManager::RetrieveModuleFileName(HANDLE hProcess, HMODULE hModule)
    {
        // NOTE: Use parentheses () not braces {} - braces create initializer_list with 2 chars!
        std::wstring wPath(MAX_PATH, L'\0');
        DWORD result = ::GetModuleFileNameExW(hProcess, hModule, wPath.data(), static_cast<DWORD>(wPath.size()));
        if (result == 0)
        {
            LogWin32Error("GetModuleFileNameExW", "module {:#x}", reinterpret_cast<uintptr_t>(hModule));
            return "";
        }
        wPath.resize(result);
        return pserv::utils::WideToUtf8(wPath);
    }

    void ModuleManager::EnumerateModules(DataObjectContainer *doc, uint32_t processId)
    {
        wil::unique_handle hProcess{OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId)};
        if (!hProcess)
        {
            // Common for system processes or elevated processes. Just log as debug.
            LogExpectedWin32Error("OpenProcess", "process {}", processId);
            return;
        }

        std::vector<HMODULE> hModules(1024); // Start with a reasonable buffer size
        DWORD cbNeeded;

        if (!::EnumProcessModules(hProcess.get(), hModules.data(), static_cast<DWORD>(hModules.size()) * sizeof(HMODULE), &cbNeeded))
        {
            LogWin32Error("EnumProcessModules", "process {}", processId);
            return;
        }

        hModules.resize(cbNeeded / sizeof(HMODULE));

        for (HMODULE hModule : hModules)
        {
            // First, get the full path (we need this as our cache key)
            // NOTE: Use parentheses () not braces {} - braces create initializer_list with 2 chars!
            std::wstring wPath(MAX_PATH, L'\0');
            DWORD pathResult = ::GetModuleFileNameExW(hProcess.get(), hModule, wPath.data(), static_cast<DWORD>(wPath.size()));
            if (pathResult == 0)
            {
                LogWin32Error("GetModuleFileNameExW", "module {:#x} in process {}", reinterpret_cast<uintptr_t>(hModule), processId);
                continue;
            }
            wPath.resize(pathResult);

            // Check cache
            auto cacheIt = s_moduleCache.find(wPath);
            if (cacheIt != s_moduleCache.end())
            {
                // Cache hit! Use cached data, only need base address from this process
                MODULEINFO moduleInfo{};
                if (::GetModuleInformation(hProcess.get(), hModule, &moduleInfo, sizeof(moduleInfo)))
                {
                    const CachedModuleInfo &cached = cacheIt->second;
                    const auto stableId{ModuleInfo::GetStableID(processId, cached.name)};
                    auto mi = doc->GetByStableId<ModuleInfo>(stableId);
                    if (mi == nullptr)
                    {
                        mi = doc->Append<ModuleInfo>(DBG_NEW ModuleInfo{processId, cached.name});
                    }
                    mi->SetValues(moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage, cached.path);
                }
            }
            else
            {
                // Cache miss - do full enumeration
                MODULEINFO moduleInfo{};
                if (::GetModuleInformation(hProcess.get(), hModule, &moduleInfo, sizeof(moduleInfo)))
                {
                    const auto moduleName = RetrieveModuleBaseName(hProcess.get(), hModule);
                    const auto modulePath = pserv::utils::WideToUtf8(wPath);

                    // Only add if we got at least a name or path
                    if (!moduleName.empty() || !modulePath.empty())
                    {
                        // Add to cache
                        CachedModuleInfo cached;
                        cached.name = moduleName;
                        cached.path = modulePath;
                        cached.size = moduleInfo.SizeOfImage;
                        s_moduleCache[wPath] = cached;

                        const auto stableId{ModuleInfo::GetStableID(processId, moduleName)};
                        auto mi = doc->GetByStableId<ModuleInfo>(stableId);
                        if (mi == nullptr)
                        {
                            mi = doc->Append<ModuleInfo>(DBG_NEW ModuleInfo{processId, moduleName});
                        }
                        mi->SetValues(moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage, modulePath);
                    }
                }
                else
                {
                    LogWin32Error("GetModuleInformation", "module {:#x} in process {}", reinterpret_cast<uintptr_t>(hModule), processId);
                }
            }
        }
    }

} // namespace pserv
