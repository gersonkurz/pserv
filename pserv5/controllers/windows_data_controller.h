#pragma once
#include "../core/data_controller.h"
#include "../models/window_info.h"
#include <vector>

namespace pserv {

enum class WindowAction {
    Show = 0,
    Hide,
    Minimize,
    Maximize,
    Restore,
    Close,
    BringToFront,
    Separator = -1
};

class WindowsDataController : public DataController {
public:
    WindowsDataController();
    virtual ~WindowsDataController();

    // Base class overrides
    void Refresh() override;
    const std::vector<DataObjectColumn>& GetColumns() const override;
    const std::vector<DataObject*>& GetDataObjects() const override;
    
    void Sort(int columnIndex, bool ascending) override;
    VisualState GetVisualState(const DataObject* dataObject) const override;
    
    std::vector<int> GetAvailableActions(const DataObject* dataObject) const override;
    std::string GetActionName(int action) const override;
    void DispatchAction(int action, DataActionDispatchContext& context) override;
    void RenderPropertiesDialog() override;

private:
    std::vector<WindowInfo*> m_windows;
    std::vector<DataObject*> m_dataObjects; // Base pointers for DataController interface
    
    void Clear();
};

} // namespace pserv
