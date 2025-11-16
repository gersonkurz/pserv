#include "precomp.h"
#include "services_data_controller.h"
#include "../windows_api/service_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace pserv {

ServicesDataController::ServicesDataController()
    : DataController("Services", "Service")
{
    // Define columns
    m_columns = {
        DataObjectColumn("Display Name", "DisplayName"),
        DataObjectColumn("Name", "Name"),
        DataObjectColumn("Status", "Status"),
        DataObjectColumn("Start Type", "StartType"),
        DataObjectColumn("Process ID", "ProcessId")
    };
}

ServicesDataController::~ServicesDataController() {
    Clear();
}

void ServicesDataController::Refresh() {
    spdlog::info("Refreshing services...");

    // Clear existing services
    Clear();

    try {
        // Enumerate services
        ServiceManager sm;
        m_services = sm.EnumerateServices();

        spdlog::info("Successfully refreshed {} services", m_services.size());

        // Re-apply last sort order if any
        if (m_lastSortColumn >= 0) {
            Sort(m_lastSortColumn, m_lastSortAscending);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to refresh services: {}", e.what());
        throw;
    }
}

const std::vector<DataObjectColumn>& ServicesDataController::GetColumns() const {
    return m_columns;
}

void ServicesDataController::Sort(int columnIndex, bool ascending) {
    if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size())) {
        spdlog::warn("Invalid column index for sorting: {}", columnIndex);
        return;
    }

    // Remember last sort for refresh
    m_lastSortColumn = columnIndex;
    m_lastSortAscending = ascending;

    spdlog::debug("Sorting by column {} ({})", columnIndex, ascending ? "ascending" : "descending");

    std::sort(m_services.begin(), m_services.end(), [columnIndex, ascending](const ServiceInfo* a, const ServiceInfo* b) {
        std::string valA = a->GetProperty(columnIndex);
        std::string valB = b->GetProperty(columnIndex);

        // For numeric columns (Process ID), do numeric comparison
        if (columnIndex == 4) { // Process ID column
            int numA = valA.empty() ? 0 : std::stoi(valA);
            int numB = valB.empty() ? 0 : std::stoi(valB);
            return ascending ? (numA < numB) : (numA > numB);
        }

        // String comparison for other columns
        int cmp = valA.compare(valB);
        return ascending ? (cmp < 0) : (cmp > 0);
    });
}

void ServicesDataController::Clear() {
    for (auto* service : m_services) {
        delete service;
    }
    m_services.clear();
}

std::vector<ServiceAction> ServicesDataController::GetAvailableActions(const ServiceInfo* service) const {
    std::vector<ServiceAction> actions;

    if (!service) {
        return actions;
    }

    // Always available actions
    actions.push_back(ServiceAction::CopyName);
    actions.push_back(ServiceAction::CopyDisplayName);

    // Get service state and capabilities
    DWORD currentState = service->GetCurrentState();
    DWORD controlsAccepted = service->GetControlsAccepted();

    // State-dependent actions
    if (currentState == SERVICE_STOPPED) {
        actions.push_back(ServiceAction::Start);
    }
    else if (currentState == SERVICE_RUNNING) {
        actions.push_back(ServiceAction::Stop);
        actions.push_back(ServiceAction::Restart);

        // Only offer Pause if service accepts pause/continue
        if (controlsAccepted & SERVICE_ACCEPT_PAUSE_CONTINUE) {
            actions.push_back(ServiceAction::Pause);
        }
    }
    else if (currentState == SERVICE_PAUSED) {
        actions.push_back(ServiceAction::Resume);
        actions.push_back(ServiceAction::Stop);
    }

    return actions;
}

VisualState ServicesDataController::GetVisualState(const ServiceInfo* service) const {
    if (!service) {
        return VisualState::Normal;
    }

    // Check if service is disabled
    DWORD startType = service->GetStartType();
    if (startType == SERVICE_DISABLED) {
        return VisualState::Disabled;
    }

    // Check if service is running
    DWORD currentState = service->GetCurrentState();
    if (currentState == SERVICE_RUNNING) {
        return VisualState::Highlighted;
    }

    return VisualState::Normal;
}

std::string ServicesDataController::GetActionName(ServiceAction action) {
    switch (action) {
    case ServiceAction::Start:
        return "Start Service";
    case ServiceAction::Stop:
        return "Stop Service";
    case ServiceAction::Restart:
        return "Restart Service";
    case ServiceAction::Pause:
        return "Pause Service";
    case ServiceAction::Resume:
        return "Resume Service";
    case ServiceAction::CopyName:
        return "Copy Name";
    case ServiceAction::CopyDisplayName:
        return "Copy Display Name";
    default:
        return "Unknown Action";
    }
}

} // namespace pserv
