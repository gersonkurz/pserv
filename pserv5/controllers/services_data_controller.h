#pragma once
#include <core/data_controller.h>
#include <models/service_info.h>
#include <dialogs/service_properties_dialog.h>

namespace pserv {

// Todo: really we should refactor actions to be objects that also implement their own dispatch function
enum class ServiceAction : int32_t {
    Start = 0,
    Stop,
    Restart,
    Pause,
    Resume,
    CopyName,
    CopyDisplayName,
    CopyBinaryPath,
    SetStartupAutomatic,
    SetStartupManual,
    SetStartupDisabled,
    OpenInRegistryEditor,
    OpenInExplorer,
    OpenTerminalHere,
    UninstallService,
    DeleteRegistryKey,
    Properties,
    Separator = -1  // Special marker for menu separators
};


class ServicesDataController : public DataController {
private:
    std::vector<ServiceInfo*> m_services;
    int m_lastSortColumn{-1};
    bool m_lastSortAscending{true};
    DWORD m_serviceType{SERVICE_WIN32 | SERVICE_DRIVER};  // Default: all services
    ServicePropertiesDialog* m_pPropertiesDialog{ nullptr };  // Service properties dialog

public:
    ServicesDataController();
    ServicesDataController(DWORD serviceType, const char* viewName, const char* itemName);
    ~ServicesDataController() override;

public:
    // DataController interface
    void Refresh() override;
    const std::vector<DataObjectColumn>& GetColumns() const override;
    void Sort(int columnIndex, bool ascending) override;

    // Get service objects
    const std::vector<ServiceInfo*>& GetServices() const { return m_services; }

    const std::vector<DataObject*>& GetDataObjects() const override
    {
		return reinterpret_cast<const std::vector<DataObject*>&>(m_services);
    }

    // Get available actions for a service
    std::vector<int> GetAvailableActions(const DataObject* service) const override;
    std::string GetActionName(int action) const override;

    // Get visual state for a service (for coloring)
    VisualState GetVisualState(const DataObject* service) const override;
    void DispatchAction(int action, DataActionDispatchContext& dispatchContext) override;
    void RenderPropertiesDialog() override;

    // Get display name for an action

    // Clear all services
    void Clear();
};

} // namespace pserv
