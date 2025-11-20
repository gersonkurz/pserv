#pragma once
#include "../core/data_controller.h"
#include "../models/process_info.h"
#include "../dialogs/process_properties_dialog.h"
#include <vector>

namespace pserv {

class ProcessesDataController : public DataController {
public:
    ProcessesDataController();
    ~ProcessesDataController() override;

    // DataController interface
    void Refresh() override;	std::vector<std::shared_ptr<DataAction>> GetActions() const override;


    std::vector<int> GetAvailableActions(const DataObject* dataObject) const override;
    void DispatchAction(int action, DataActionDispatchContext& context) override;
    std::string GetActionName(int action) const override;
    VisualState GetVisualState(const DataObject* dataObject) const override;

    const std::vector<DataObject*>& GetDataObjects() const override
    {
        return reinterpret_cast<const std::vector<DataObject*>&>(m_processes);
    }

    void RenderPropertiesDialog() override;

private:
    void Clear();

    std::vector<ProcessInfo*> m_processes;
    ProcessPropertiesDialog* m_pPropertiesDialog{ nullptr };

    std::string m_currentUserName;
};

enum class ProcessAction {
    Terminate = 0,
    Properties,
    Separator = -1,
    SetPriorityRealtime = 100,
    SetPriorityHigh,
    SetPriorityAboveNormal,
    SetPriorityNormal,
    SetPriorityBelowNormal,
    SetPriorityLow,
    OpenLocation
};

} // namespace pserv
