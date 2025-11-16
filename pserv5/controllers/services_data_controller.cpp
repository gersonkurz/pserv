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

} // namespace pserv
