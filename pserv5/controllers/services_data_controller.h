#pragma once
#include "../core/data_controller.h"
#include "../models/service_info.h"
#include <vector>
#include <memory>

namespace pserv {

class ServicesDataController : public DataController {
private:
    std::vector<ServiceInfo*> m_services;

public:
    ServicesDataController();
    ~ServicesDataController() override;

    // DataController interface
    void Refresh() override;
    const std::vector<DataObjectColumn>& GetColumns() const override;
    void Sort(int columnIndex, bool ascending) override;

    // Get service objects
    const std::vector<ServiceInfo*>& GetServices() const { return m_services; }

    // Clear all services
    void Clear();
};

} // namespace pserv
