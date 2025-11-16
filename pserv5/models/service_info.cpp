#include "precomp.h"
#include "service_info.h"
#include <format>

namespace pserv {

ServiceInfo::ServiceInfo(std::string name, std::string displayName, DWORD currentState, DWORD serviceType)
    : m_name(std::move(name))
    , m_displayName(std::move(displayName))
    , m_currentState(currentState)
    , m_startType(0)
    , m_processId(0)
    , m_serviceType(serviceType)
{
    // Update running state based on service state
    SetRunning(m_currentState == SERVICE_RUNNING);
}

void ServiceInfo::SetCurrentState(DWORD state) {
    m_currentState = state;
    SetRunning(m_currentState == SERVICE_RUNNING);
}

void ServiceInfo::Update(const DataObject& other) {
    const auto* otherService = dynamic_cast<const ServiceInfo*>(&other);
    if (!otherService) return;

    m_displayName = otherService->m_displayName;
    SetCurrentState(otherService->m_currentState);
    m_startType = otherService->m_startType;
    m_processId = otherService->m_processId;
    m_serviceType = otherService->m_serviceType;
    m_binaryPathName = otherService->m_binaryPathName;
    m_description = otherService->m_description;
}

std::string ServiceInfo::GetProperty(int propertyId) const {
    switch (static_cast<ServiceProperty>(propertyId)) {
        case ServiceProperty::Name:
            return m_name;
        case ServiceProperty::DisplayName:
            return m_displayName;
        case ServiceProperty::Status:
            return GetStatusString();
        case ServiceProperty::StartType:
            return GetStartTypeString();
        case ServiceProperty::ProcessId:
            return m_processId > 0 ? std::to_string(m_processId) : "";
        case ServiceProperty::ServiceType:
            return std::to_string(m_serviceType);
        case ServiceProperty::BinaryPathName:
            return m_binaryPathName;
        case ServiceProperty::Description:
            return m_description;
        default:
            return "";
    }
}

std::string ServiceInfo::GetStatusString() const {
    switch (m_currentState) {
        case SERVICE_STOPPED: return "Stopped";
        case SERVICE_START_PENDING: return "Start Pending";
        case SERVICE_STOP_PENDING: return "Stop Pending";
        case SERVICE_RUNNING: return "Running";
        case SERVICE_CONTINUE_PENDING: return "Continue Pending";
        case SERVICE_PAUSE_PENDING: return "Pause Pending";
        case SERVICE_PAUSED: return "Paused";
        default: return std::format("Unknown ({})", m_currentState);
    }
}

std::string ServiceInfo::GetStartTypeString() const {
    switch (m_startType) {
        case SERVICE_AUTO_START: return "Automatic";
        case SERVICE_BOOT_START: return "Boot";
        case SERVICE_DEMAND_START: return "Manual";
        case SERVICE_DISABLED: return "Disabled";
        case SERVICE_SYSTEM_START: return "System";
        default: return std::format("Unknown ({})", m_startType);
    }
}

} // namespace pserv
