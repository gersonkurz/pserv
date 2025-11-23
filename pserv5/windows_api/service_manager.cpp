#include "precomp.h"
#include <models/service_info.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/service_manager.h>
#include <core/data_object_container.h>

namespace pserv
{

    namespace
    {
        // Track services that have logged QueryServiceConfig2W errors to avoid repeated logging
        std::unordered_set<std::string> g_servicesWithLoggedErrors;

        std::string GetServiceStateString(DWORD state)
        {
            switch (state)
            {
            case SERVICE_STOPPED:
                return "Stopped";
            case SERVICE_START_PENDING:
                return "Start Pending";
            case SERVICE_STOP_PENDING:
                return "Stop Pending";
            case SERVICE_RUNNING:
                return "Running";
            case SERVICE_CONTINUE_PENDING:
                return "Continue Pending";
            case SERVICE_PAUSE_PENDING:
                return "Pause Pending";
            case SERVICE_PAUSED:
                return "Paused";
            default:
                return std::format("Unknown ({})", state);
            }
        }
    } // namespace

    ServiceManager::ServiceManager()
    {
        m_hScManager.reset(OpenSCManagerW(nullptr, // local machine
            nullptr,                               // default database
            SC_MANAGER_ENUMERATE_SERVICE));

        if (!m_hScManager)
        {
            LogWin32Error("OpenSCManagerW");
            // Don't throw - keep object usable but methods will return empty data
        }
    }

    void ServiceManager::EnumerateServices(DataObjectContainer *doc, DWORD serviceType, bool isAutoRefresh)
    {
        if (!m_hScManager)
        {
            spdlog::warn("Service Control Manager not available");
            return;
        }

        // First call to get required buffer size
        DWORD bytesNeeded = 0;
        DWORD servicesReturned = 0;
        DWORD resumeHandle = 0;

        EnumServicesStatusExW(m_hScManager.get(),
            SC_ENUM_PROCESS_INFO,
            serviceType, // Use the specified service type filter
            SERVICE_STATE_ALL,
            nullptr,
            0,
            &bytesNeeded,
            &servicesReturned,
            &resumeHandle,
            nullptr);

        if (GetLastError() != ERROR_MORE_DATA)
        {
            LogWin32Error("EnumServicesStatusExW", "size query");
            return;
        }

        // Allocate buffer and enumerate
        std::vector<BYTE> buffer(bytesNeeded);
        auto *pServices = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW *>(buffer.data());

        if (!EnumServicesStatusExW(m_hScManager.get(),
                SC_ENUM_PROCESS_INFO,
                serviceType, // Use the specified service type filter
                SERVICE_STATE_ALL,
                buffer.data(),
                bytesNeeded,
                &bytesNeeded,
                &servicesReturned,
                &resumeHandle,
                nullptr))
        {
            LogWin32Error("EnumServicesStatusExW");
            return;
        }

        for (DWORD i = 0; i < servicesReturned; ++i)
        {
            const auto serviceName{utils::WideToUtf8(pServices[i].lpServiceName)};
            const auto stableId{ServiceInfo::GetStableID(serviceName)};
            auto info = doc->GetByStableId<ServiceInfo>(stableId);
            if (info == nullptr)
            {
                info = doc->Append<ServiceInfo>(DBG_NEW ServiceInfo{serviceName});
            }
            info->SetValues(utils::WideToUtf8(pServices[i].lpDisplayName),
                pServices[i].ServiceStatusProcess.dwCurrentState,
                pServices[i].ServiceStatusProcess.dwServiceType);

            info->SetProcessId(pServices[i].ServiceStatusProcess.dwProcessId);
            info->SetControlsAccepted(pServices[i].ServiceStatusProcess.dwControlsAccepted);
            info->SetWin32ExitCode(pServices[i].ServiceStatusProcess.dwWin32ExitCode);
            info->SetServiceSpecificExitCode(pServices[i].ServiceStatusProcess.dwServiceSpecificExitCode);
            info->SetCheckPoint(pServices[i].ServiceStatusProcess.dwCheckPoint);
            info->SetWaitHint(pServices[i].ServiceStatusProcess.dwWaitHint);
            info->SetServiceFlags(pServices[i].ServiceStatusProcess.dwServiceFlags);

            // Query service configuration to get additional details
            SC_HANDLE hService = OpenServiceW(m_hScManager.get(), pServices[i].lpServiceName, SERVICE_QUERY_CONFIG);

            if (!hService)
            {
                LogWin32Error("OpenServiceW", "service '{}'", utils::WideToUtf8(pServices[i].lpServiceName));
            }
            else
            {
                DWORD bytesNeeded = 0;
                BOOL result = QueryServiceConfigW(hService, nullptr, 0, &bytesNeeded);
                DWORD error = GetLastError();

                if (!result && error != ERROR_INSUFFICIENT_BUFFER)
                {
                    LogWin32Error("QueryServiceConfigW", "size query for '{}'", utils::WideToUtf8(pServices[i].lpServiceName));
                }
                else if (error == ERROR_INSUFFICIENT_BUFFER)
                {
                    std::vector<BYTE> configBuffer(bytesNeeded);
                    auto *pConfig = reinterpret_cast<QUERY_SERVICE_CONFIGW *>(configBuffer.data());

                    if (!QueryServiceConfigW(hService, pConfig, bytesNeeded, &bytesNeeded))
                    {
                        LogWin32Error("QueryServiceConfigW", "service '{}'", utils::WideToUtf8(pServices[i].lpServiceName));
                    }
                    else
                    {
                        info->SetStartType(pConfig->dwStartType);
                        info->SetErrorControl(pConfig->dwErrorControl);
                        info->SetTagId(pConfig->dwTagId);

                        if (pConfig->lpBinaryPathName)
                        {
                            info->SetBinaryPathName(utils::WideToUtf8(pConfig->lpBinaryPathName));
                        }
                        if (pConfig->lpLoadOrderGroup)
                        {
                            info->SetLoadOrderGroup(utils::WideToUtf8(pConfig->lpLoadOrderGroup));
                        }
                        if (pConfig->lpServiceStartName)
                        {
                            info->SetUser(utils::WideToUtf8(pConfig->lpServiceStartName));
                        }
                    }
                }

                // Query service description
                bytesNeeded = 0;
                result = QueryServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, &bytesNeeded);
                error = GetLastError();

                if (!result && error != ERROR_INSUFFICIENT_BUFFER)
                {
                    const auto serviceName = utils::WideToUtf8(pServices[i].lpServiceName);
                    // Only log error once per service, or always if not auto-refresh
                    if (!isAutoRefresh || g_servicesWithLoggedErrors.find(serviceName) == g_servicesWithLoggedErrors.end())
                    {
                        LogWin32Error("QueryServiceConfig2W", "size query for '{}'", serviceName);
                        g_servicesWithLoggedErrors.insert(serviceName);
                    }
                }
                else if (error == ERROR_INSUFFICIENT_BUFFER)
                {
                    std::vector<BYTE> descBuffer(bytesNeeded);
                    auto *pDesc = reinterpret_cast<SERVICE_DESCRIPTIONW *>(descBuffer.data());

                    if (!QueryServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, descBuffer.data(), bytesNeeded, &bytesNeeded))
                    {
                        LogWin32Error("QueryServiceConfig2W", "service '{}'", utils::WideToUtf8(pServices[i].lpServiceName));
                    }
                    else
                    {
                        if (pDesc->lpDescription)
                        {
                            info->SetDescription(utils::WideToUtf8(pDesc->lpDescription));
                        }
                    }
                }

                CloseServiceHandle(hService);
            }
        }

        if (!isAutoRefresh)
            spdlog::info("Enumerated {} services", doc->GetSize());
    }

    bool ServiceManager::StartServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback)
    {
        spdlog::info("Starting service: {}", serviceName);

        // Open SCM
        SC_HANDLE hScManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!hScManager)
        {
            LogWin32Error("OpenSCManagerW");
            return false;
        }

        // Convert service name to wide string
        std::wstring wServiceName = utils::Utf8ToWide(serviceName);

        // Open service
        SC_HANDLE hService = OpenServiceW(hScManager, wServiceName.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
        if (!hService)
        {
            CloseServiceHandle(hScManager);
            LogWin32Error("OpenServiceW", "service '{}'", serviceName);
            return false;
        }

        // Start the service
        BOOL result = StartServiceW(hService, 0, nullptr);
        DWORD error = GetLastError();

        if (!result && error != ERROR_SERVICE_ALREADY_RUNNING)
        {
            CloseServiceHandle(hService);
            CloseServiceHandle(hScManager);
            LogWin32Error("StartServiceW", "service '{}'", serviceName);
            return false;
        }

        // Wait for service to reach running state
        SERVICE_STATUS_PROCESS ssp;
        DWORD bytesNeeded;
        const int maxWaitTime = 30000; // 30 seconds max
        const int pollInterval = 1000; // Check every 1 second for progress updates
        int totalWait = 0;
        DWORD estimatedWaitTime = 0;

        while (totalWait < maxWaitTime)
        {
            if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded))
            {
                CloseServiceHandle(hService);
                CloseServiceHandle(hScManager);
                LogWin32Error("QueryServiceStatusEx", "service '{}'", serviceName);
                return false;
            }

            // Get service state string
            std::string stateStr = GetServiceStateString(ssp.dwCurrentState);

            // Use wait hint for progress calculation (hint is in milliseconds)
            // Add 5 second buffer because dwWaitHint is often unreliable
            if (ssp.dwWaitHint > 0)
            {
                estimatedWaitTime = ssp.dwWaitHint + 5000;
            }
            else
            {
                estimatedWaitTime = maxWaitTime; // Default if no hint
            }

            // Calculate progress based on elapsed time vs estimated time
            float progress = std::min(0.95f, static_cast<float>(totalWait) / static_cast<float>(estimatedWaitTime));

            // Report progress
            if (progressCallback)
            {
                progressCallback(progress, std::format("Service state: {}", stateStr));
            }

            if (ssp.dwCurrentState == SERVICE_RUNNING)
            {
                if (progressCallback)
                {
                    progressCallback(1.0f, "Service is running");
                }
                break;
            }

            if (ssp.dwCurrentState == SERVICE_STOPPED)
            {
                CloseServiceHandle(hService);
                CloseServiceHandle(hScManager);
                spdlog::error("Service '{}' stopped unexpectedly during start", serviceName);
                return false;
            }

            Sleep(pollInterval);
            totalWait += pollInterval;
        }

        CloseServiceHandle(hService);
        CloseServiceHandle(hScManager);

        if (totalWait >= maxWaitTime)
        {
            spdlog::warn("Service '{}' did not reach running state within timeout", serviceName);
            return false;
        }

        spdlog::info("Service '{}' started successfully", serviceName);
        return true;
    }

    bool ServiceManager::StopServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback)
    {
        spdlog::info("Stopping service: {}", serviceName);

        // Open SCM
        SC_HANDLE hScManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!hScManager)
        {
            LogWin32Error("OpenSCManagerW");
            return false;
        }

        // Convert service name to wide string
        std::wstring wServiceName = utils::Utf8ToWide(serviceName);

        // Open service
        SC_HANDLE hService = OpenServiceW(hScManager, wServiceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (!hService)
        {
            CloseServiceHandle(hScManager);
            LogWin32Error("OpenServiceW", "service '{}'", serviceName);
            return false;
        }

        // Stop the service
        SERVICE_STATUS_PROCESS ssp;
        BOOL result = ControlService(hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp);
        DWORD error = GetLastError();

        if (!result && error != ERROR_SERVICE_NOT_ACTIVE)
        {
            CloseServiceHandle(hService);
            CloseServiceHandle(hScManager);
            LogWin32Error("ControlService(STOP)", "service '{}'", serviceName);
            return false;
        }

        // Wait for service to reach stopped state
        DWORD bytesNeeded;
        const int maxWaitTime = 30000; // 30 seconds max
        const int pollInterval = 1000; // Check every 1 second for progress updates
        int totalWait = 0;
        DWORD estimatedWaitTime = 0;

        while (totalWait < maxWaitTime)
        {
            if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded))
            {
                CloseServiceHandle(hService);
                CloseServiceHandle(hScManager);
                LogWin32Error("QueryServiceStatusEx", "service '{}'", serviceName);
                return false;
            }

            // Get service state string
            std::string stateStr = GetServiceStateString(ssp.dwCurrentState);

            // Use wait hint for progress calculation (hint is in milliseconds)
            // Add 5 second buffer because dwWaitHint is often unreliable
            if (ssp.dwWaitHint > 0)
            {
                estimatedWaitTime = ssp.dwWaitHint + 5000;
            }
            else
            {
                estimatedWaitTime = maxWaitTime; // Default if no hint
            }

            // Calculate progress based on elapsed time vs estimated time
            float progress = std::min(0.95f, static_cast<float>(totalWait) / static_cast<float>(estimatedWaitTime));

            // Report progress
            if (progressCallback)
            {
                progressCallback(progress, std::format("Service state: {}", stateStr));
            }

            if (ssp.dwCurrentState == SERVICE_STOPPED)
            {
                if (progressCallback)
                {
                    progressCallback(1.0f, "Service is stopped");
                }
                break;
            }

            Sleep(pollInterval);
            totalWait += pollInterval;
        }

        CloseServiceHandle(hService);
        CloseServiceHandle(hScManager);

        if (totalWait >= maxWaitTime)
        {
            spdlog::warn("Service '{}' did not reach stopped state within timeout", serviceName);
            return false;
        }

        spdlog::info("Service '{}' stopped successfully", serviceName);
        return true;
    }

    bool ServiceManager::PauseServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback)
    {
        spdlog::info("Pausing service: {}", serviceName);

        // Open SCM
        SC_HANDLE hScManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!hScManager)
        {
            LogWin32Error("OpenSCManagerW");
            return false;
        }

        // Convert service name to wide string
        std::wstring wServiceName = utils::Utf8ToWide(serviceName);

        // Open service
        SC_HANDLE hService = OpenServiceW(hScManager, wServiceName.c_str(), SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS);
        if (!hService)
        {
            CloseServiceHandle(hScManager);
            LogWin32Error("OpenServiceW", "service '{}'", serviceName);
            return false;
        }

        // Pause the service
        SERVICE_STATUS_PROCESS ssp;
        BOOL result = ControlService(hService, SERVICE_CONTROL_PAUSE, (LPSERVICE_STATUS)&ssp);
        DWORD error = GetLastError();

        if (!result)
        {
            CloseServiceHandle(hService);
            CloseServiceHandle(hScManager);
            LogWin32Error("ControlService(PAUSE)", "service '{}'", serviceName);
            return false;
        }

        // Wait for service to reach paused state
        DWORD bytesNeeded;
        const int maxWaitTime = 30000;
        const int pollInterval = 1000;
        int totalWait = 0;
        DWORD estimatedWaitTime = 0;

        while (totalWait < maxWaitTime)
        {
            if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded))
            {
                CloseServiceHandle(hService);
                CloseServiceHandle(hScManager);
                LogWin32Error("QueryServiceStatusEx", "service '{}'", serviceName);
                return false;
            }

            std::string stateStr = GetServiceStateString(ssp.dwCurrentState);

            if (ssp.dwWaitHint > 0)
            {
                estimatedWaitTime = ssp.dwWaitHint + 5000;
            }
            else
            {
                estimatedWaitTime = maxWaitTime;
            }

            float progress = std::min(0.95f, static_cast<float>(totalWait) / static_cast<float>(estimatedWaitTime));

            if (progressCallback)
            {
                progressCallback(progress, std::format("Service state: {}", stateStr));
            }

            if (ssp.dwCurrentState == SERVICE_PAUSED)
            {
                if (progressCallback)
                {
                    progressCallback(1.0f, "Service is paused");
                }
                break;
            }

            Sleep(pollInterval);
            totalWait += pollInterval;
        }

        CloseServiceHandle(hService);
        CloseServiceHandle(hScManager);

        if (totalWait >= maxWaitTime)
        {
            spdlog::warn("Service '{}' did not reach paused state within timeout", serviceName);
            return false;
        }

        spdlog::info("Service '{}' paused successfully", serviceName);
        return true;
    }

    bool ServiceManager::ResumeServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback)
    {
        spdlog::info("Resuming service: {}", serviceName);

        // Open SCM
        SC_HANDLE hScManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!hScManager)
        {
            LogWin32Error("OpenSCManagerW");
            return false;
        }

        // Convert service name to wide string
        std::wstring wServiceName = utils::Utf8ToWide(serviceName);

        // Open service
        SC_HANDLE hService = OpenServiceW(hScManager, wServiceName.c_str(), SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS);
        if (!hService)
        {
            CloseServiceHandle(hScManager);
            LogWin32Error("OpenServiceW", "service '{}'", serviceName);
            return false;
        }

        // Resume the service
        SERVICE_STATUS_PROCESS ssp;
        BOOL result = ControlService(hService, SERVICE_CONTROL_CONTINUE, (LPSERVICE_STATUS)&ssp);
        DWORD error = GetLastError();

        if (!result)
        {
            CloseServiceHandle(hService);
            CloseServiceHandle(hScManager);
            LogWin32Error("ControlService(CONTINUE)", "service '{}'", serviceName);
            return false;
        }

        // Wait for service to reach running state
        DWORD bytesNeeded;
        const int maxWaitTime = 30000;
        const int pollInterval = 1000;
        int totalWait = 0;
        DWORD estimatedWaitTime = 0;

        while (totalWait < maxWaitTime)
        {
            if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded))
            {
                CloseServiceHandle(hService);
                CloseServiceHandle(hScManager);
                LogWin32Error("QueryServiceStatusEx", "service '{}'", serviceName);
                return false;
            }

            std::string stateStr = GetServiceStateString(ssp.dwCurrentState);

            if (ssp.dwWaitHint > 0)
            {
                estimatedWaitTime = ssp.dwWaitHint + 5000;
            }
            else
            {
                estimatedWaitTime = maxWaitTime;
            }

            float progress = std::min(0.95f, static_cast<float>(totalWait) / static_cast<float>(estimatedWaitTime));

            if (progressCallback)
            {
                progressCallback(progress, std::format("Service state: {}", stateStr));
            }

            if (ssp.dwCurrentState == SERVICE_RUNNING)
            {
                if (progressCallback)
                {
                    progressCallback(1.0f, "Service is running");
                }
                break;
            }

            Sleep(pollInterval);
            totalWait += pollInterval;
        }

        CloseServiceHandle(hService);
        CloseServiceHandle(hScManager);

        if (totalWait >= maxWaitTime)
        {
            spdlog::warn("Service '{}' did not reach running state within timeout", serviceName);
            return false;
        }

        spdlog::info("Service '{}' resumed successfully", serviceName);
        return true;
    }

    bool ServiceManager::RestartServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback)
    {
        spdlog::info("Restarting service: {}", serviceName);

        // Stop the service (0-50% progress)
        if (progressCallback)
        {
            progressCallback(0.0f, "Stopping service...");
        }

        StopServiceByName(serviceName,
            [progressCallback](float progress, std::string message)
            {
                if (progressCallback)
                {
                    // Map 0-100% to 0-50%
                    progressCallback(progress * 0.5f, message);
                }
            });

        // Start the service (50-100% progress)
        if (progressCallback)
        {
            progressCallback(0.5f, "Starting service...");
        }

        StartServiceByName(serviceName,
            [progressCallback](float progress, std::string message)
            {
                if (progressCallback)
                {
                    // Map 0-100% to 50-100%
                    progressCallback(0.5f + progress * 0.5f, message);
                }
            });

        spdlog::info("Service '{}' restarted successfully", serviceName);
        return true;
    }

    bool ServiceManager::ChangeServiceStartType(const std::string &serviceName, DWORD startType)
    {
        spdlog::info("Changing startup type for service '{}' to {}", serviceName, startType);

        // Convert service name to wide string
        std::wstring wServiceName = utils::Utf8ToWide(serviceName);

        // Open SC Manager
        wil::unique_schandle hScManager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
        if (!hScManager)
        {
            LogWin32Error("OpenSCManagerW");
            return false;
        }

        // Open the service with change config access
        wil::unique_schandle hService(OpenServiceW(hScManager.get(), wServiceName.c_str(), SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG));

        if (!hService)
        {
            LogWin32Error("OpenServiceW", "service '{}'", serviceName);
            return false;
        }

        // Change the service configuration
        if (!::ChangeServiceConfigW(hService.get(),
                SERVICE_NO_CHANGE, // dwServiceType
                startType,         // dwStartType
                SERVICE_NO_CHANGE, // dwErrorControl
                nullptr,           // lpBinaryPathName
                nullptr,           // lpLoadOrderGroup
                nullptr,           // lpdwTagId
                nullptr,           // lpDependencies
                nullptr,           // lpServiceStartName
                nullptr,           // lpPassword
                nullptr            // lpDisplayName
                ))
        {
            LogWin32Error("ChangeServiceConfigW", "service '{}'", serviceName);
            return false;
        }

        spdlog::info("Service '{}' startup type changed successfully", serviceName);
        return true;
    }

    bool ServiceManager::DeleteService(const std::string &serviceName)
    {
        spdlog::info("Deleting service '{}'", serviceName);

        // Convert service name to wide string
        std::wstring wServiceName = utils::Utf8ToWide(serviceName);

        // Open SC Manager
        wil::unique_schandle hScManager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
        if (!hScManager)
        {
            LogWin32Error("OpenSCManagerW");
            return false;
        }

        // Open the service with delete access
        wil::unique_schandle hService(OpenServiceW(hScManager.get(), wServiceName.c_str(), DELETE));

        if (!hService)
        {
            LogWin32Error("OpenServiceW", "service '{}'", serviceName);
            return false;
        }

        // Delete the service
        if (!::DeleteService(hService.get()))
        {
            LogWin32Error("DeleteService", "service '{}'", serviceName);
            return false;
        }

        spdlog::info("Service '{}' deleted successfully", serviceName);
        return true;
    }

    bool ServiceManager::ChangeServiceConfig(
        const std::string &serviceName, const std::string &displayName, const std::string &description, DWORD startType, const std::string &binaryPathName)
    {
        spdlog::info("Changing configuration for service '{}'", serviceName);

        // Convert strings to wide strings
        std::wstring wServiceName = utils::Utf8ToWide(serviceName);
        std::wstring wDisplayName = utils::Utf8ToWide(displayName);
        std::wstring wDescription = utils::Utf8ToWide(description);
        std::wstring wBinaryPathName = utils::Utf8ToWide(binaryPathName);

        // Open SC Manager
        wil::unique_schandle hScManager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
        if (!hScManager)
        {
            LogWin32Error("OpenSCManagerW");
            return false;
        }

        // Open the service with change config access
        wil::unique_schandle hService(OpenServiceW(hScManager.get(), wServiceName.c_str(), SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG));

        if (!hService)
        {
            LogWin32Error("OpenServiceW", "service '{}'", serviceName);
            return false;
        }

        // Change the service configuration
        if (!::ChangeServiceConfigW(hService.get(),
                SERVICE_NO_CHANGE,                                           // dwServiceType
                startType,                                                   // dwStartType
                SERVICE_NO_CHANGE,                                           // dwErrorControl
                wBinaryPathName.empty() ? nullptr : wBinaryPathName.c_str(), // lpBinaryPathName
                nullptr,                                                     // lpLoadOrderGroup
                nullptr,                                                     // lpdwTagId
                nullptr,                                                     // lpDependencies
                nullptr,                                                     // lpServiceStartName
                nullptr,                                                     // lpPassword
                wDisplayName.empty() ? nullptr : wDisplayName.c_str()        // lpDisplayName
                ))
        {
            LogWin32Error("ChangeServiceConfigW", "service '{}'", serviceName);
            return false;
        }

        // Update description using ChangeServiceConfig2W
        if (!description.empty())
        {
            SERVICE_DESCRIPTIONW sd;
            sd.lpDescription = const_cast<wchar_t *>(wDescription.c_str());

            if (!::ChangeServiceConfig2W(hService.get(), SERVICE_CONFIG_DESCRIPTION, &sd))
            {
                LogWin32Error("ChangeServiceConfig2W", "service '{}' description", serviceName);
                return false;
            }
        }

        spdlog::info("Service '{}' configuration changed successfully", serviceName);
        return true;
    }

} // namespace pserv
