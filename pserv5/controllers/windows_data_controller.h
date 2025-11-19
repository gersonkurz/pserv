#pragma once
#include "../core/data_controller.h"
#include "../models/window_info.h"
#include "../dialogs/window_properties_dialog.h"
#include <vector>

namespace pserv {

enum class WindowAction {
    Properties,  // New
    Separator1,  // Placeholder for logic
    Show,
    Hide,
    Minimize,
    Maximize,
    Restore,
    Separator2,  // Placeholder for logic
    BringToFront,
    Close,
    Separator = -1
};

class WindowsDataController : public DataController {
public:
    WindowsDataController();
    virtual ~WindowsDataController();

    // Base class overrides
    void Refresh() override;
    const std::vector<DataObject*>& GetDataObjects() const override;

    VisualState GetVisualState(const DataObject* dataObject) const override;
    
    std::vector<int> GetAvailableActions(const DataObject* dataObject) const override;
    std::string GetActionName(int action) const override;
    void DispatchAction(int action, DataActionDispatchContext& context) override;
    void RenderPropertiesDialog() override;

private:
    std::vector<WindowInfo*> m_windows;
    std::vector<DataObject*> m_dataObjects;
    WindowPropertiesDialog* m_pPropertiesDialog;
    
    void Clear();
};

} // namespace pserv
