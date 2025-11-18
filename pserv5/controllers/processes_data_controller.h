#pragma once
#include "../core/data_controller.h"
#include "../models/process_info.h"
#include <vector>

namespace pserv {

class ProcessesDataController : public DataController {
public:
    ProcessesDataController();
    ~ProcessesDataController() override;

    // DataController interface
    void Refresh() override;
    const std::vector<DataObjectColumn>& GetColumns() const override;
    std::vector<int> GetAvailableActions(const DataObject* dataObject) const override;
    void DispatchAction(int action, DataActionDispatchContext& context) override;
    std::string GetActionName(int action) const override;
    VisualState GetVisualState(const DataObject* dataObject) const override;

    const std::vector<DataObject*>& GetDataObjects() const override
    {
        if (m_processes.empty())
        {
            const_cast<ProcessesDataController*>(this)->Refresh();
        }
        return reinterpret_cast<const std::vector<DataObject*>&>(m_processes);
    }

    void RenderPropertiesDialog() override;

private:
    void Clear();
    void Sort(int columnIndex, bool ascending) override;

    std::vector<ProcessInfo*> m_processes;
    std::vector<DataObjectColumn> m_columns;
    
    // Sorting state
    int m_lastSortColumn{-1};
    bool m_lastSortAscending{true};
    
    std::string m_currentUserName;
};

enum class ProcessAction {
    Terminate = 0,
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
