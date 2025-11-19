#pragma once
#include "../core/data_controller.h"
#include "../models/module_info.h"
#include "../windows_api/module_manager.h"
#include <vector>

namespace pserv {

enum class ModuleAction {
    CopyInfo = 0,
    OpenContainingFolder,
    Properties, // TBD: Implement ModulePropertiesDialog later
    Separator = -1
};

class ModulesDataController : public DataController {
public:
    ModulesDataController();
    ~ModulesDataController() override;

    // DataController interface
    void Refresh() override;
    const std::vector<DataObjectColumn>& GetColumns() const override;
    const std::vector<DataObject*>& GetDataObjects() const override;
    std::vector<int> GetAvailableActions(const DataObject* dataObject) const override;
    std::string GetActionName(int action) const override;
    VisualState GetVisualState(const DataObject* dataObject) const override;
    void DispatchAction(int action, DataActionDispatchContext& context) override;
    void RenderPropertiesDialog() override;
    void Sort(int columnIndex, bool ascending) override;

private:
    std::vector<DataObjectColumn> m_columns;
    std::vector<ModuleInfo*> m_modules;

    void Clear();
};

} // namespace pserv
