#pragma once
#include <core/data_controller.h>
#include <models/service_info.h>

namespace pserv {

class ServicesDataController : public DataController {
private:
    DWORD m_serviceType{SERVICE_WIN32 | SERVICE_DRIVER};  // Default: all services

public:
    ServicesDataController();
    ServicesDataController(DWORD serviceType, const char* viewName, const char* itemName);

private:
    // DataController interface
    void Refresh() override;
    std::vector<const DataAction*> GetActions(const DataObject* dataObject) const override;

    // Get visual state for a service (for coloring)
    VisualState GetVisualState(const DataObject* service) const override;
};

} // namespace pserv
