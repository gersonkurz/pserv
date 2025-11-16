#include "precomp.h"
#include "service_manager.h"
#include "../models/service_info.h"
#include "../utils/string_utils.h"
#include <spdlog/spdlog.h>

namespace pserv {

namespace {
    std::string GetServiceStateString(DWORD state) {
        switch (state) {
            case SERVICE_STOPPED: return "Stopped";
            case SERVICE_START_PENDING: return "Start Pending";
            case SERVICE_STOP_PENDING: return "Stop Pending";
            case SERVICE_RUNNING: return "Running";
            case SERVICE_CONTINUE_PENDING: return "Continue Pending";
            case SERVICE_PAUSE_PENDING: return "Pause Pending";
            case SERVICE_PAUSED: return "Paused";
            default: return std::format("Unknown ({})", state);
        }
    }
}


ServiceManager::ServiceManager() {
    m_hScManager.reset(OpenSCManagerW(
        nullptr,  // local machine
        nullptr,  // default database
        SC_MANAGER_ENUMERATE_SERVICE
    ));

    if (!m_hScManager) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open Service Control Manager: error {}", error);
        throw std::runtime_error("Failed to open Service Control Manager");
    }
}

std::vector<ServiceInfo*> ServiceManager::EnumerateServices() {
    std::vector<ServiceInfo*> services;

    // First call to get required buffer size
    DWORD bytesNeeded = 0;
    DWORD servicesReturned = 0;
    DWORD resumeHandle = 0;

    EnumServicesStatusExW(
        m_hScManager.get(),
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        nullptr,
        0,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        nullptr
    );

    if (GetLastError() != ERROR_MORE_DATA) {
        spdlog::error("EnumServicesStatusEx failed unexpectedly: error {}", GetLastError());
        return services;
    }

    // Allocate buffer and enumerate
    std::vector<BYTE> buffer(bytesNeeded);
    auto* pServices = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());

    if (!EnumServicesStatusExW(
        m_hScManager.get(),
        SC_ENUM_PROCESS_INFO,
        SERVICE_WIN32,
        SERVICE_STATE_ALL,
        buffer.data(),
        bytesNeeded,
        &bytesNeeded,
        &servicesReturned,
        &resumeHandle,
        nullptr
    )) {
        spdlog::error("EnumServicesStatusEx failed: error {}", GetLastError());
        return services;
    }

    // Convert to ServiceInfo objects
    services.reserve(servicesReturned);
    for (DWORD i = 0; i < servicesReturned; ++i) {
        auto* info = new ServiceInfo(
            utils::WideToUtf8(pServices[i].lpServiceName),
            utils::WideToUtf8(pServices[i].lpDisplayName),
            pServices[i].ServiceStatusProcess.dwCurrentState,
            pServices[i].ServiceStatusProcess.dwServiceType
        );
        info->SetProcessId(pServices[i].ServiceStatusProcess.dwProcessId);
        info->SetControlsAccepted(pServices[i].ServiceStatusProcess.dwControlsAccepted);
        info->SetWin32ExitCode(pServices[i].ServiceStatusProcess.dwWin32ExitCode);
        info->SetServiceSpecificExitCode(pServices[i].ServiceStatusProcess.dwServiceSpecificExitCode);
        info->SetCheckPoint(pServices[i].ServiceStatusProcess.dwCheckPoint);
        info->SetWaitHint(pServices[i].ServiceStatusProcess.dwWaitHint);
        info->SetServiceFlags(pServices[i].ServiceStatusProcess.dwServiceFlags);

        // Query service configuration to get additional details
        SC_HANDLE hService = OpenServiceW(
            m_hScManager.get(),
            pServices[i].lpServiceName,
            SERVICE_QUERY_CONFIG
        );

        if (hService) {
            DWORD bytesNeeded = 0;
            QueryServiceConfigW(hService, nullptr, 0, &bytesNeeded);

            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<BYTE> configBuffer(bytesNeeded);
                auto* pConfig = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(configBuffer.data());

                if (QueryServiceConfigW(hService, pConfig, bytesNeeded, &bytesNeeded)) {
                    info->SetStartType(pConfig->dwStartType);
                    info->SetErrorControl(pConfig->dwErrorControl);
                    info->SetTagId(pConfig->dwTagId);

                    if (pConfig->lpBinaryPathName) {
                        info->SetBinaryPathName(utils::WideToUtf8(pConfig->lpBinaryPathName));
                    }
                    if (pConfig->lpLoadOrderGroup) {
                        info->SetLoadOrderGroup(utils::WideToUtf8(pConfig->lpLoadOrderGroup));
                    }
                    if (pConfig->lpServiceStartName) {
                        info->SetUser(utils::WideToUtf8(pConfig->lpServiceStartName));
                    }
                }
            }

            // Query service description
            bytesNeeded = 0;
            QueryServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, &bytesNeeded);

            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                std::vector<BYTE> descBuffer(bytesNeeded);
                auto* pDesc = reinterpret_cast<SERVICE_DESCRIPTIONW*>(descBuffer.data());

                if (QueryServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, descBuffer.data(), bytesNeeded, &bytesNeeded)) {
                    if (pDesc->lpDescription) {
                        info->SetDescription(utils::WideToUtf8(pDesc->lpDescription));
                    }
                }
            }

            CloseServiceHandle(hService);
        }

        services.push_back(info);
    }

    spdlog::info("Enumerated {} services", services.size());
    return services;
}

bool ServiceManager::StartServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback) {
    spdlog::info("Starting service: {}", serviceName);

    // Open SCM
    SC_HANDLE hScManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hScManager) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open Service Control Manager: error {}", error);
        throw std::runtime_error(std::format("Failed to open SCM: error {}", error));
    }

    // Convert service name to wide string
    std::wstring wServiceName = utils::Utf8ToWide(serviceName);

    // Open service
    SC_HANDLE hService = OpenServiceW(hScManager, wServiceName.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
    if (!hService) {
        DWORD error = GetLastError();
        CloseServiceHandle(hScManager);
        spdlog::error("Failed to open service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to open service: error {}", error));
    }

    // Start the service
    BOOL result = StartServiceW(hService, 0, nullptr);
    DWORD error = GetLastError();

    if (!result && error != ERROR_SERVICE_ALREADY_RUNNING) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hScManager);
        spdlog::error("Failed to start service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to start service: error {}", error));
    }

    // Wait for service to reach running state
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded;
    const int maxWaitTime = 30000; // 30 seconds max
    const int pollInterval = 1000; // Check every 1 second for progress updates
    int totalWait = 0;
    DWORD estimatedWaitTime = 0;

    while (totalWait < maxWaitTime) {
        if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded)) {
            error = GetLastError();
            CloseServiceHandle(hService);
            CloseServiceHandle(hScManager);
            spdlog::error("Failed to query service status: error {}", error);
            throw std::runtime_error(std::format("Failed to query service status: error {}", error));
        }

        // Get service state string
        std::string stateStr = GetServiceStateString(ssp.dwCurrentState);

        // Use wait hint for progress calculation (hint is in milliseconds)
        // Add 5 second buffer because dwWaitHint is often unreliable
        if (ssp.dwWaitHint > 0) {
            estimatedWaitTime = ssp.dwWaitHint + 5000;
        } else {
            estimatedWaitTime = maxWaitTime; // Default if no hint
        }

        // Calculate progress based on elapsed time vs estimated time
        float progress = std::min(0.95f, static_cast<float>(totalWait) / static_cast<float>(estimatedWaitTime));

        // Report progress
        if (progressCallback) {
            progressCallback(progress, std::format("Service state: {}", stateStr));
        }

        if (ssp.dwCurrentState == SERVICE_RUNNING) {
            if (progressCallback) {
                progressCallback(1.0f, "Service is running");
            }
            break;
        }

        if (ssp.dwCurrentState == SERVICE_STOPPED) {
            CloseServiceHandle(hService);
            CloseServiceHandle(hScManager);
            spdlog::error("Service '{}' stopped unexpectedly during start", serviceName);
            throw std::runtime_error("Service stopped unexpectedly during start");
        }

        Sleep(pollInterval);
        totalWait += pollInterval;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hScManager);

    if (totalWait >= maxWaitTime) {
        spdlog::warn("Service '{}' did not reach running state within timeout", serviceName);
        throw std::runtime_error("Service start timed out");
    }

    spdlog::info("Service '{}' started successfully", serviceName);
    return true;
}

bool ServiceManager::StopServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback) {
    spdlog::info("Stopping service: {}", serviceName);

    // Open SCM
    SC_HANDLE hScManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hScManager) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open Service Control Manager: error {}", error);
        throw std::runtime_error(std::format("Failed to open SCM: error {}", error));
    }

    // Convert service name to wide string
    std::wstring wServiceName = utils::Utf8ToWide(serviceName);

    // Open service
    SC_HANDLE hService = OpenServiceW(hScManager, wServiceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!hService) {
        DWORD error = GetLastError();
        CloseServiceHandle(hScManager);
        spdlog::error("Failed to open service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to open service: error {}", error));
    }

    // Stop the service
    SERVICE_STATUS_PROCESS ssp;
    BOOL result = ControlService(hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp);
    DWORD error = GetLastError();

    if (!result && error != ERROR_SERVICE_NOT_ACTIVE) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hScManager);
        spdlog::error("Failed to stop service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to stop service: error {}", error));
    }

    // Wait for service to reach stopped state
    DWORD bytesNeeded;
    const int maxWaitTime = 30000; // 30 seconds max
    const int pollInterval = 1000; // Check every 1 second for progress updates
    int totalWait = 0;
    DWORD estimatedWaitTime = 0;

    while (totalWait < maxWaitTime) {
        if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded)) {
            error = GetLastError();
            CloseServiceHandle(hService);
            CloseServiceHandle(hScManager);
            spdlog::error("Failed to query service status: error {}", error);
            throw std::runtime_error(std::format("Failed to query service status: error {}", error));
        }

        // Get service state string
        std::string stateStr = GetServiceStateString(ssp.dwCurrentState);

        // Use wait hint for progress calculation (hint is in milliseconds)
        // Add 5 second buffer because dwWaitHint is often unreliable
        if (ssp.dwWaitHint > 0) {
            estimatedWaitTime = ssp.dwWaitHint + 5000;
        } else {
            estimatedWaitTime = maxWaitTime; // Default if no hint
        }

        // Calculate progress based on elapsed time vs estimated time
        float progress = std::min(0.95f, static_cast<float>(totalWait) / static_cast<float>(estimatedWaitTime));

        // Report progress
        if (progressCallback) {
            progressCallback(progress, std::format("Service state: {}", stateStr));
        }

        if (ssp.dwCurrentState == SERVICE_STOPPED) {
            if (progressCallback) {
                progressCallback(1.0f, "Service is stopped");
            }
            break;
        }

        Sleep(pollInterval);
        totalWait += pollInterval;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hScManager);

    if (totalWait >= maxWaitTime) {
        spdlog::warn("Service '{}' did not reach stopped state within timeout", serviceName);
        throw std::runtime_error("Service stop timed out");
    }

    spdlog::info("Service '{}' stopped successfully", serviceName);
    return true;
}

bool ServiceManager::PauseServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback) {
    spdlog::info("Pausing service: {}", serviceName);

    // Open SCM
    SC_HANDLE hScManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hScManager) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open Service Control Manager: error {}", error);
        throw std::runtime_error(std::format("Failed to open SCM: error {}", error));
    }

    // Convert service name to wide string
    std::wstring wServiceName = utils::Utf8ToWide(serviceName);

    // Open service
    SC_HANDLE hService = OpenServiceW(hScManager, wServiceName.c_str(), SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS);
    if (!hService) {
        DWORD error = GetLastError();
        CloseServiceHandle(hScManager);
        spdlog::error("Failed to open service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to open service: error {}", error));
    }

    // Pause the service
    SERVICE_STATUS_PROCESS ssp;
    BOOL result = ControlService(hService, SERVICE_CONTROL_PAUSE, (LPSERVICE_STATUS)&ssp);
    DWORD error = GetLastError();

    if (!result) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hScManager);
        spdlog::error("Failed to pause service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to pause service: error {}", error));
    }

    // Wait for service to reach paused state
    DWORD bytesNeeded;
    const int maxWaitTime = 30000;
    const int pollInterval = 1000;
    int totalWait = 0;
    DWORD estimatedWaitTime = 0;

    while (totalWait < maxWaitTime) {
        if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded)) {
            error = GetLastError();
            CloseServiceHandle(hService);
            CloseServiceHandle(hScManager);
            spdlog::error("Failed to query service status: error {}", error);
            throw std::runtime_error(std::format("Failed to query service status: error {}", error));
        }

        std::string stateStr = GetServiceStateString(ssp.dwCurrentState);

        if (ssp.dwWaitHint > 0) {
            estimatedWaitTime = ssp.dwWaitHint + 5000;
        } else {
            estimatedWaitTime = maxWaitTime;
        }

        float progress = std::min(0.95f, static_cast<float>(totalWait) / static_cast<float>(estimatedWaitTime));

        if (progressCallback) {
            progressCallback(progress, std::format("Service state: {}", stateStr));
        }

        if (ssp.dwCurrentState == SERVICE_PAUSED) {
            if (progressCallback) {
                progressCallback(1.0f, "Service is paused");
            }
            break;
        }

        Sleep(pollInterval);
        totalWait += pollInterval;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hScManager);

    if (totalWait >= maxWaitTime) {
        spdlog::warn("Service '{}' did not reach paused state within timeout", serviceName);
        throw std::runtime_error("Service pause timed out");
    }

    spdlog::info("Service '{}' paused successfully", serviceName);
    return true;
}

bool ServiceManager::ResumeServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback) {
    spdlog::info("Resuming service: {}", serviceName);

    // Open SCM
    SC_HANDLE hScManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!hScManager) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open Service Control Manager: error {}", error);
        throw std::runtime_error(std::format("Failed to open SCM: error {}", error));
    }

    // Convert service name to wide string
    std::wstring wServiceName = utils::Utf8ToWide(serviceName);

    // Open service
    SC_HANDLE hService = OpenServiceW(hScManager, wServiceName.c_str(), SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS);
    if (!hService) {
        DWORD error = GetLastError();
        CloseServiceHandle(hScManager);
        spdlog::error("Failed to open service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to open service: error {}", error));
    }

    // Resume the service
    SERVICE_STATUS_PROCESS ssp;
    BOOL result = ControlService(hService, SERVICE_CONTROL_CONTINUE, (LPSERVICE_STATUS)&ssp);
    DWORD error = GetLastError();

    if (!result) {
        CloseServiceHandle(hService);
        CloseServiceHandle(hScManager);
        spdlog::error("Failed to resume service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to resume service: error {}", error));
    }

    // Wait for service to reach running state
    DWORD bytesNeeded;
    const int maxWaitTime = 30000;
    const int pollInterval = 1000;
    int totalWait = 0;
    DWORD estimatedWaitTime = 0;

    while (totalWait < maxWaitTime) {
        if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded)) {
            error = GetLastError();
            CloseServiceHandle(hService);
            CloseServiceHandle(hScManager);
            spdlog::error("Failed to query service status: error {}", error);
            throw std::runtime_error(std::format("Failed to query service status: error {}", error));
        }

        std::string stateStr = GetServiceStateString(ssp.dwCurrentState);

        if (ssp.dwWaitHint > 0) {
            estimatedWaitTime = ssp.dwWaitHint + 5000;
        } else {
            estimatedWaitTime = maxWaitTime;
        }

        float progress = std::min(0.95f, static_cast<float>(totalWait) / static_cast<float>(estimatedWaitTime));

        if (progressCallback) {
            progressCallback(progress, std::format("Service state: {}", stateStr));
        }

        if (ssp.dwCurrentState == SERVICE_RUNNING) {
            if (progressCallback) {
                progressCallback(1.0f, "Service is running");
            }
            break;
        }

        Sleep(pollInterval);
        totalWait += pollInterval;
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hScManager);

    if (totalWait >= maxWaitTime) {
        spdlog::warn("Service '{}' did not reach running state within timeout", serviceName);
        throw std::runtime_error("Service resume timed out");
    }

    spdlog::info("Service '{}' resumed successfully", serviceName);
    return true;
}

bool ServiceManager::RestartServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback) {
    spdlog::info("Restarting service: {}", serviceName);

    // Stop the service (0-50% progress)
    if (progressCallback) {
        progressCallback(0.0f, "Stopping service...");
    }

    StopServiceByName(serviceName, [progressCallback](float progress, std::string message) {
        if (progressCallback) {
            // Map 0-100% to 0-50%
            progressCallback(progress * 0.5f, message);
        }
    });

    // Start the service (50-100% progress)
    if (progressCallback) {
        progressCallback(0.5f, "Starting service...");
    }

    StartServiceByName(serviceName, [progressCallback](float progress, std::string message) {
        if (progressCallback) {
            // Map 0-100% to 50-100%
            progressCallback(0.5f + progress * 0.5f, message);
        }
    });

    spdlog::info("Service '{}' restarted successfully", serviceName);
    return true;
}

bool ServiceManager::ChangeServiceStartType(const std::string& serviceName, DWORD startType) {
    spdlog::info("Changing startup type for service '{}' to {}", serviceName, startType);

    // Convert service name to wide string
    std::wstring wServiceName = utils::Utf8ToWide(serviceName);

    // Open SC Manager
    wil::unique_schandle hScManager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!hScManager) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open SC Manager: error {}", error);
        throw std::runtime_error(std::format("Failed to open SC Manager: error {}", error));
    }

    // Open the service with change config access
    wil::unique_schandle hService(OpenServiceW(
        hScManager.get(),
        wServiceName.c_str(),
        SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG
    ));

    if (!hService) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to open service '{}': error {}", serviceName, error));
    }

    // Change the service configuration
    if (!ChangeServiceConfigW(
        hService.get(),
        SERVICE_NO_CHANGE,  // dwServiceType
        startType,          // dwStartType
        SERVICE_NO_CHANGE,  // dwErrorControl
        nullptr,            // lpBinaryPathName
        nullptr,            // lpLoadOrderGroup
        nullptr,            // lpdwTagId
        nullptr,            // lpDependencies
        nullptr,            // lpServiceStartName
        nullptr,            // lpPassword
        nullptr             // lpDisplayName
    )) {
        DWORD error = GetLastError();
        spdlog::error("Failed to change service '{}' start type: error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to change service start type: error {}", error));
    }

    spdlog::info("Service '{}' startup type changed successfully", serviceName);
    return true;
}

bool ServiceManager::DeleteService(const std::string& serviceName) {
    spdlog::info("Deleting service '{}'", serviceName);

    // Convert service name to wide string
    std::wstring wServiceName = utils::Utf8ToWide(serviceName);

    // Open SC Manager
    wil::unique_schandle hScManager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
    if (!hScManager) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open SC Manager: error {}", error);
        throw std::runtime_error(std::format("Failed to open SC Manager: error {}", error));
    }

    // Open the service with delete access
    wil::unique_schandle hService(OpenServiceW(
        hScManager.get(),
        wServiceName.c_str(),
        DELETE
    ));

    if (!hService) {
        DWORD error = GetLastError();
        spdlog::error("Failed to open service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to open service '{}': error {}", serviceName, error));
    }

    // Delete the service
    if (!::DeleteService(hService.get())) {
        DWORD error = GetLastError();
        spdlog::error("Failed to delete service '{}': error {}", serviceName, error);
        throw std::runtime_error(std::format("Failed to delete service: error {}", error));
    }

    spdlog::info("Service '{}' deleted successfully", serviceName);
    return true;
}

} // namespace pserv
