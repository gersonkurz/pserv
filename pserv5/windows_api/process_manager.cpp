#include "precomp.h"
#include "process_manager.h"
#include "../models/process_info.h"
#include <utils/string_utils.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <sddl.h>
#include <wil/resource.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

namespace pserv {

// Helper to get user from process handle
static std::string GetProcessUser(HANDLE hProcess) {
    wil::unique_handle hToken;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        return "";
    }

    DWORD len = 0;
    GetTokenInformation(hToken.get(), TokenUser, nullptr, 0, &len);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return "";
    }

    auto buffer = std::make_unique<BYTE[]>(len);
    if (!GetTokenInformation(hToken.get(), TokenUser, buffer.get(), len, &len)) {
        return "";
    }

    TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(buffer.get());
    
    // Resolve SID to name
    wchar_t name[256];
    wchar_t domain[256];
    DWORD nameLen = sizeof(name) / sizeof(wchar_t);
    DWORD domainLen = sizeof(domain) / sizeof(wchar_t);
    SID_NAME_USE type;

    if (LookupAccountSidW(nullptr, user->User.Sid, name, &nameLen, domain, &domainLen, &type)) {
        return std::format("{}\\{}", utils::WideToUtf8(domain), utils::WideToUtf8(name));
    }
    
    return "";
}

// Helper to get path
static std::string GetProcessPathInternal(HANDLE hProcess) {
    char buffer[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameA(hProcess, 0, buffer, &size)) {
        return std::string(buffer, size);
    }
    return "";
}

std::vector<ProcessInfo*> ProcessManager::EnumerateProcesses() {
    std::vector<ProcessInfo*> processes;

    wil::unique_handle hSnapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!hSnapshot) {
        spdlog::error("CreateToolhelp32Snapshot failed: {}", GetLastError());
        return processes;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    if (!Process32FirstW(hSnapshot.get(), &pe32)) {
        spdlog::error("Process32FirstW failed: {}", GetLastError());
        return processes;
    }

    do {
        std::string name = utils::WideToUtf8(pe32.szExeFile);

        auto* pProcess = new ProcessInfo(pe32.th32ProcessID, name);
        pProcess->SetParentPid(pe32.th32ParentProcessID);
        pProcess->SetThreadCount(pe32.cntThreads);
        
        // Open process to get more info
        // PROCESS_QUERY_LIMITED_INFORMATION is enough for Token, Path, Priority, etc. on Vista+
        wil::unique_handle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID));
        
        if (hProcess) {
            pProcess->SetUser(GetProcessUser(hProcess.get()));
            pProcess->SetPath(GetProcessPathInternal(hProcess.get()));
            pProcess->SetPriorityClass(GetPriorityClass(hProcess.get()));
            pProcess->SetHandleCount(0); // Requires GetProcessHandleCount usually

            PROCESS_MEMORY_COUNTERS_EX pmc;
            if (GetProcessMemoryInfo(hProcess.get(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                pProcess->SetWorkingSetSize(pmc.WorkingSetSize);
                pProcess->SetPeakWorkingSetSize(pmc.PeakWorkingSetSize);
                pProcess->SetPrivatePageCount(pmc.PrivateUsage);
            }
        } else {
            // Access denied or system process
             if (pe32.th32ProcessID == 0 || pe32.th32ProcessID == 4) {
                 pProcess->SetUser("SYSTEM");
             }
        }

        processes.push_back(pProcess);

    } while (Process32NextW(hSnapshot.get(), &pe32));

    return processes;
}

bool ProcessManager::TerminateProcessById(DWORD pid) {
    wil::unique_handle hProcess(OpenProcess(PROCESS_TERMINATE, FALSE, pid));
    if (!hProcess) {
        spdlog::error("OpenProcess for terminate failed: {}", GetLastError());
        return false;
    }

    if (!TerminateProcess(hProcess.get(), 1)) {
        spdlog::error("TerminateProcess failed: {}", GetLastError());
        return false;
    }
    return true;
}

bool ProcessManager::SetProcessPriority(DWORD pid, DWORD priorityClass) {
    wil::unique_handle hProcess(OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid));
    if (!hProcess) {
        spdlog::error("OpenProcess for SetPriority failed: {}", GetLastError());
        return false;
    }

    if (!SetPriorityClass(hProcess.get(), priorityClass)) {
        spdlog::error("SetPriorityClass failed: {}", GetLastError());
        return false;
    }
    return true;
}

std::string ProcessManager::GetProcessPath(DWORD pid) {
    wil::unique_handle hProcess(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (hProcess) {
        return GetProcessPathInternal(hProcess.get());
    }
    return "";
}

} // namespace pserv
