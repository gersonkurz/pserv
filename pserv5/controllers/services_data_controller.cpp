#include "precomp.h"
#include <actions/service_actions.h>
#include <controllers/services_data_controller.h>
#include <core/async_operation.h>
#include <utils/string_utils.h>
#include <windows_api/service_manager.h>

#include <imgui.h>

namespace pserv
{

    ServicesDataController::ServicesDataController(DWORD serviceType, const char *viewName, const char *itemName)
        : DataController{viewName ? viewName : "Services",
              itemName ? itemName : "Service",
              {{"Display Name", "DisplayName", ColumnDataType::String},
                  {"Name", "Name", ColumnDataType::String},
                  {"Status", "Status", ColumnDataType::String},
                  {"Start Type", "StartType", ColumnDataType::String},
                  {"Process ID", "ProcessId", ColumnDataType::UnsignedInteger},
                  {"Service Type", "ServiceType", ColumnDataType::String},
                  {"Binary Path Name", "BinaryPathName", ColumnDataType::String},
                  {"Description", "Description", ColumnDataType::String},
                  {"User", "User", ColumnDataType::String},
                  {"Load Order Group", "LoadOrderGroup", ColumnDataType::String},
                  {"Error Control", "ErrorControl", ColumnDataType::String},
                  {"Tag ID", "TagId", ColumnDataType::UnsignedInteger},
                  {"Win32 Exit Code", "Win32ExitCode", ColumnDataType::UnsignedInteger},
                  {"Service Specific Exit Code", "ServiceSpecificExitCode", ColumnDataType::UnsignedInteger},
                  {"Check Point", "CheckPoint", ColumnDataType::UnsignedInteger},
                  {"Wait Hint", "WaitHint", ColumnDataType::UnsignedInteger},
                  {"Service Flags", "ServiceFlags", ColumnDataType::UnsignedInteger},
                  {"Controls Accepted", "ControlsAccepted", ColumnDataType::String}}},
          m_serviceType{serviceType}
    {
    }

    void ServicesDataController::Refresh()
    {
        spdlog::info("Refreshing services...");

        // Clear existing services
        Clear();

        try
        {
            // Enumerate services with the configured service type filter
            ServiceManager sm;
            m_objects = sm.EnumerateServices(m_serviceType);

            spdlog::info("Successfully refreshed {} services", m_objects.size());

            // Re-apply last sort order if any
            if (m_lastSortColumn >= 0)
            {
                Sort(m_lastSortColumn, m_lastSortAscending);
            }

            m_bLoaded = true;
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to refresh services: {}", e.what());
            throw;
        }
    }

    std::vector<const DataAction *> ServicesDataController::GetActions(const DataObject *dataObject) const
    {

        const auto service = dynamic_cast<const ServiceInfo *>(dataObject);
        const auto currentState = service->GetCurrentState();
        const auto controlsAccepted = service->GetControlsAccepted();
        return CreateServiceActions(currentState, controlsAccepted);
    }

    VisualState ServicesDataController::GetVisualState(const DataObject *dataObject) const
    {
        if (!dataObject)
        {
            return VisualState::Normal;
        }

        const auto service = dynamic_cast<const ServiceInfo *>(dataObject);

        // Check if service is disabled
        DWORD startType = service->GetStartType();
        if (startType == SERVICE_DISABLED)
        {
            return VisualState::Disabled;
        }

        // Check if service is running
        DWORD currentState = service->GetCurrentState();
        if (currentState == SERVICE_RUNNING)
        {
            return VisualState::Highlighted;
        }

        return VisualState::Normal;
    }

} // namespace pserv
