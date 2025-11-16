#include "precomp.h"
#include "service_manager.h"
#include "../models/service_info.h"
#include "../utils/string_utils.h"
#include <spdlog/spdlog.h>

namespace pserv {

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
        services.push_back(info);
    }

    spdlog::info("Enumerated {} services", services.size());
    return services;
}

} // namespace pserv
