#pragma once
#include <core/data_controller.h>

namespace pserv
{

    class ServicesDataController : public DataController
    {
    private:
        const DWORD m_serviceType{SERVICE_WIN32 | SERVICE_DRIVER}; // Default: all services

    public:
        ServicesDataController(DWORD serviceType = SERVICE_WIN32, const char *viewName = nullptr, const char *itemName = nullptr);

    private:
        // DataController interface
        void Refresh() override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

        // Get visual state for a service (for coloring)
        VisualState GetVisualState(const DataObject *service) const override;
    };

} // namespace pserv
