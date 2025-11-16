#pragma once
#include "../core/data_controller.h"
#include "../models/service_info.h"
#include <vector>
#include <memory>
#include <string>

namespace pserv {

enum class ServiceAction {
    Start,
    Stop,
    Restart,
    Pause,
    Resume,
    CopyName,
    CopyDisplayName
};

enum class VisualState {
    Normal,      // Default text color
    Highlighted, // Special highlight (e.g., running services, own processes)
    Disabled     // Grayed out (e.g., disabled services, inaccessible processes)
};

class ServicesDataController : public DataController {
private:
    std::vector<ServiceInfo*> m_services;
    int m_lastSortColumn{-1};
    bool m_lastSortAscending{true};

public:
    ServicesDataController();
    ~ServicesDataController() override;

    // DataController interface
    void Refresh() override;
    const std::vector<DataObjectColumn>& GetColumns() const override;
    void Sort(int columnIndex, bool ascending) override;

    // Get service objects
    const std::vector<ServiceInfo*>& GetServices() const { return m_services; }

    // Get available actions for a service
    std::vector<ServiceAction> GetAvailableActions(const ServiceInfo* service) const;

    // Get visual state for a service (for coloring)
    VisualState GetVisualState(const ServiceInfo* service) const;

    // Get display name for an action
    static std::string GetActionName(ServiceAction action);

    // Clear all services
    void Clear();
};

} // namespace pserv
