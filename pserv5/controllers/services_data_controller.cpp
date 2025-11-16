#include "precomp.h"
#include "services_data_controller.h"
#include "../windows_api/service_manager.h"
#include <spdlog/spdlog.h>

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
    } catch (const std::exception& e) {
        spdlog::error("Failed to refresh services: {}", e.what());
        throw;
    }
}

const std::vector<DataObjectColumn>& ServicesDataController::GetColumns() const {
    return m_columns;
}

void ServicesDataController::Clear() {
    for (auto* service : m_services) {
        delete service;
    }
    m_services.clear();
}

} // namespace pserv
