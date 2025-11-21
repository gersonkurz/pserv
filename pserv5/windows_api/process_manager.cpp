#include "precomp.h"
#include <windows_api/process_manager.h>
#include <models/process_info.h>
#include <utils/string_utils.h>

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

// Definitions for PEB access
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

// From winternl.h
typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation = 0,
    ProcessWow64Information = 26
} PROCESSINFOCLASS;

typedef NTSTATUS(NTAPI* pfnNtQueryInformationProcess)(
    HANDLE ProcessHandle,
    PROCESSINFOCLASS ProcessInformationClass,
    PVOID ProcessInformation,
    ULONG ProcessInformationLength,
    PULONG ReturnLength
    );

// Partial definitions from winternl.h / documented structures
// We need UNICODE_STRING and RTL_USER_PROCESS_PARAMETERS layout

typedef struct _UNICODE_STRING_T {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING_T;

// Simplified structures for what we need
// Architecture agnostic offset calculation is better, but let's define strict layouts for x64/x86

// 64-bit PEB structures
struct PROCESS_BASIC_INFORMATION64 {
    NTSTATUS ExitStatus;
    DWORD64 PebBaseAddress;
    DWORD64 AffinityMask;
    DWORD64 BasePriority;
    DWORD64 UniqueProcessId;
    DWORD64 InheritedFromUniqueProcessId;
};

// 32-bit PEB structures (for WoW64)
struct PROCESS_BASIC_INFORMATION32 {
    NTSTATUS ExitStatus;
    DWORD32 PebBaseAddress;
    DWORD32 AffinityMask;
    DWORD32 BasePriority;
    DWORD32 UniqueProcessId;
    DWORD32 InheritedFromUniqueProcessId;
};

static std::string GetProcessCommandLine(HANDLE hProcess) {
    static auto NtQueryInformationProcess = (pfnNtQueryInformationProcess)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    if (!NtQueryInformationProcess) return "";

    // Determine if target is 32-bit (WoW64) or 64-bit
    BOOL bIsWow64 = FALSE;
    IsWow64Process(hProcess, &bIsWow64);

    // We are a 64-bit process (pserv5)
    
    if (bIsWow64) {
        // Target is 32-bit. We need to read its 32-bit PEB.
        // However, accessing 32-bit PEB from 64-bit process requires locating it.
        // NtQueryInformationProcess with ProcessWow64Information returns the PEB address.
        
        ULONG_PTR peb32Address = 0;
        if (NT_SUCCESS(NtQueryInformationProcess(hProcess, ProcessWow64Information, &peb32Address, sizeof(peb32Address), nullptr)) && peb32Address != 0) {
             // Read ProcessParameters address from PEB32
             // PEB32 offset 0x10 is ProcessParameters (pointer)
             
             DWORD32 processParameters32 = 0;
             if (ReadProcessMemory(hProcess, (PVOID)((char*)peb32Address + 0x10), &processParameters32, sizeof(processParameters32), nullptr)) {
                 
                 // Read CommandLine UNICODE_STRING32 from ProcessParameters32
                 // RTL_USER_PROCESS_PARAMETERS32 offset 0x40 is CommandLine
                 
                 struct {
                     USHORT Length;
                     USHORT MaximumLength;
                     DWORD32 Buffer;
                 } cmdLineString32;

                 // Cast DWORD32 to DWORD64 before casting to void* to avoid warning
                 if (ReadProcessMemory(hProcess, (PVOID)(DWORD64)((char*)(DWORD64)processParameters32 + 0x40), &cmdLineString32, sizeof(cmdLineString32), nullptr)) {
                     
                     if (cmdLineString32.Length > 0 && cmdLineString32.Buffer != 0) {
                         std::vector<wchar_t> buffer(cmdLineString32.Length / 2 + 1);
                         if (ReadProcessMemory(hProcess, (PVOID)(DWORD64)cmdLineString32.Buffer, buffer.data(), cmdLineString32.Length, nullptr)) {
                             buffer[cmdLineString32.Length / 2] = L'\0';
                             return utils::WideToUtf8(buffer.data());
                         }
                     }
                 }
             }
        }
    }
    else {
        // Target is 64-bit
        PROCESS_BASIC_INFORMATION64 pbi = { 0 };
        if (NT_SUCCESS(NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), nullptr))) {
            
            // Read ProcessParameters address from PEB
            // PEB64 offset 0x20 is ProcessParameters
            
            DWORD64 processParameters = 0;
            if (ReadProcessMemory(hProcess, (PVOID)(pbi.PebBaseAddress + 0x20), &processParameters, sizeof(processParameters), nullptr)) {
                
                // Read CommandLine UNICODE_STRING from ProcessParameters
                // RTL_USER_PROCESS_PARAMETERS64 offset 0x70 is CommandLine
                
                UNICODE_STRING_T cmdLineString;
                if (ReadProcessMemory(hProcess, (PVOID)(processParameters + 0x70), &cmdLineString, sizeof(cmdLineString), nullptr)) {
                    
                    if (cmdLineString.Length > 0 && cmdLineString.Buffer != nullptr) {
                        std::vector<wchar_t> buffer(cmdLineString.Length / 2 + 1);
                        if (ReadProcessMemory(hProcess, cmdLineString.Buffer, buffer.data(), cmdLineString.Length, nullptr)) {
                             buffer[cmdLineString.Length / 2] = L'\0';
                             return utils::WideToUtf8(buffer.data());
                        }
                    }
                }
            }
        }
    }

    return "";
}

std::vector<DataObject*> ProcessManager::EnumerateProcesses() {
    std::vector<DataObject*> processes;

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
            pProcess->SetCommandLine(GetProcessCommandLine(hProcess.get()));
            
            DWORD handleCount = 0;
            if (GetProcessHandleCount(hProcess.get(), &handleCount)) {
                pProcess->SetHandleCount(handleCount);
            }

            PROCESS_MEMORY_COUNTERS_EX pmc;
            if (GetProcessMemoryInfo(hProcess.get(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                pProcess->SetWorkingSetSize(pmc.WorkingSetSize);
                pProcess->SetPeakWorkingSetSize(pmc.PeakWorkingSetSize);
                pProcess->SetPrivatePageCount(pmc.PrivateUsage);
                
                // Set extended memory stats
                pProcess->SetMemoryExtras(pmc.QuotaPagedPoolUsage, pmc.QuotaNonPagedPoolUsage, pmc.PageFaultCount);
            }
            
            FILETIME creation, exit, kernel, user;
            if (GetProcessTimes(hProcess.get(), &creation, &exit, &kernel, &user)) {
                pProcess->SetTimes(creation, exit, kernel, user);
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
