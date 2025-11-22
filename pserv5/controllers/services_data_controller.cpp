#include "precomp.h"
#include <actions/service_actions.h>
#include <controllers/services_data_controller.h>
#include <core/async_operation.h>
#include <utils/string_utils.h>
#include <windows_api/service_manager.h>
#include <models/service_info.h>

namespace pserv
{

    ServicesDataController::ServicesDataController(DWORD serviceType, const char *viewName, const char *itemName)
        : DataController{viewName ? viewName : SERVICES_DATA_CONTROLLER_NAME,
              itemName ? itemName : "Service",
              {{"Display Name", "DisplayName", ColumnDataType::String, true, ColumnEditType::Text},
                  {"Name", "Name", ColumnDataType::String},
                  {"Status", "Status", ColumnDataType::String},
                  {"Start Type", "StartType", ColumnDataType::String, true, ColumnEditType::Combo},
                  {"Process ID", "ProcessId", ColumnDataType::UnsignedInteger},
                  {"Service Type", "ServiceType", ColumnDataType::String},
                  {"Binary Path Name", "BinaryPathName", ColumnDataType::String, true, ColumnEditType::Text},
                  {"Description", "Description", ColumnDataType::String, true, ColumnEditType::TextMultiline},
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
            m_objects.StartRefresh();
            sm.EnumerateServices(&m_objects, m_serviceType);
            m_objects.FinishRefresh();

            spdlog::info("Successfully refreshed {} services", m_objects.GetSize());

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

    void ServicesDataController::BeginPropertyEdits(DataObject *obj)
    {
        m_editingObject = obj;
        m_editBuffer = EditBuffer{}; // Clear buffer

        // Initialize buffer with current values
        auto *service = static_cast<ServiceInfo *>(obj);
        m_editBuffer.displayName = service->GetDisplayName();
        m_editBuffer.description = service->GetDescription();
        m_editBuffer.binaryPathName = service->GetBinaryPathName();
        m_editBuffer.startTypeValue = service->GetStartType();

        // Convert start type to string
        m_editBuffer.startType = service->GetStartTypeString();

        spdlog::info("BeginPropertyEdits for service: {}", service->GetName());
    }

    bool ServicesDataController::SetPropertyEdit(DataObject *obj, int columnIndex, const std::string &newValue)
    {
        if (obj != m_editingObject)
        {
            spdlog::error("SetPropertyEdit called with wrong object");
            return false;
        }

        // Column indices based on constructor order:
        // 0=DisplayName, 1=Name, 2=Status, 3=StartType, 4=ProcessId, 5=ServiceType, 6=BinaryPathName, 7=Description

        switch (columnIndex)
        {
        case 0: // DisplayName
            m_editBuffer.displayName = newValue;
            spdlog::debug("Set DisplayName = {}", newValue);
            return true;

        case 3: // StartType
        {
            m_editBuffer.startType = newValue;

            // Parse start type string to DWORD
            if (newValue == "Automatic")
                m_editBuffer.startTypeValue = SERVICE_AUTO_START;
            else if (newValue == "Manual")
                m_editBuffer.startTypeValue = SERVICE_DEMAND_START;
            else if (newValue == "Disabled")
                m_editBuffer.startTypeValue = SERVICE_DISABLED;
            else if (newValue == "Boot")
                m_editBuffer.startTypeValue = SERVICE_BOOT_START;
            else if (newValue == "System")
                m_editBuffer.startTypeValue = SERVICE_SYSTEM_START;
            else
            {
                spdlog::warn("Unknown start type: {}", newValue);
                return false;
            }

            spdlog::debug("Set StartType = {} ({})", newValue, m_editBuffer.startTypeValue);
            return true;
        }

        case 6: // BinaryPathName
            m_editBuffer.binaryPathName = newValue;
            spdlog::debug("Set BinaryPathName = {}", newValue);
            return true;

        case 7: // Description
            m_editBuffer.description = newValue;
            spdlog::debug("Set Description = {}", newValue);
            return true;

        default:
            spdlog::warn("Attempted to edit non-editable column: {}", columnIndex);
            return false;
        }
    }

    bool ServicesDataController::CommitPropertyEdits(DataObject *obj)
    {
        if (obj != m_editingObject)
        {
            spdlog::error("CommitPropertyEdits called with wrong object");
            return false;
        }

        auto *service = static_cast<ServiceInfo *>(obj);

        try
        {
            spdlog::info("Committing property edits for service: {}", service->GetName());

            // Call ChangeServiceConfig with all accumulated changes
            ServiceManager::ChangeServiceConfig(service->GetName(),
                m_editBuffer.displayName,
                m_editBuffer.description,
                m_editBuffer.startTypeValue,
                m_editBuffer.binaryPathName);

            // Update local service object with new values
            service->SetDisplayName(m_editBuffer.displayName);
            service->SetDescription(m_editBuffer.description);
            service->SetStartType(m_editBuffer.startTypeValue);
            service->SetBinaryPathName(m_editBuffer.binaryPathName);

            spdlog::info("Successfully committed property edits for service: {}", service->GetName());
            m_editingObject = nullptr;
            return true;
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to commit property edits: {}", e.what());
            m_editingObject = nullptr;
            return false;
        }
    }

    std::vector<std::string> ServicesDataController::GetComboOptions(int columnIndex) const
    {
        // Column index 3 = StartType
        if (columnIndex == 3)
        {
            return {"Automatic", "Manual", "Disabled", "Boot", "System"};
        }

        return {};
    }

} // namespace pserv
