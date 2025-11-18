#pragma once
#include "../core/data_controller.h"
#include "../models/installed_program_info.h"
#include "../dialogs/uninstaller_properties_dialog.h"
#include <vector>

namespace pserv {

// Actions specific to installed programs
enum class UninstallerAction {
    Properties = 1, // Opens a dialog with program details
    Uninstall,      // Initiates the uninstallation process
    Separator = -1  // Separator for context menus
};

class UninstallerDataController : public DataController {
public:
    UninstallerDataController();
    ~UninstallerDataController() override;

    void Refresh() override;
    void Clear();

    const std::vector<DataObjectColumn>& GetColumns() const override;
    const std::vector<DataObject*>& GetDataObjects() const override;
    VisualState GetVisualState(const DataObject* dataObject) const override; // No special states for now
    std::vector<int> GetAvailableActions(const DataObject* dataObject) const override;
    std::string GetActionName(int action) const override;
    void DispatchAction(int action, DataActionDispatchContext& context) override;
    void RenderPropertiesDialog() override;
    void Sort(int columnIndex, bool ascending) override;

private:
    std::vector<InstalledProgramInfo*> m_programs;
    std::vector<DataObject*> m_dataObjects; // Pointers to programs for base class
    UninstallerPropertiesDialog* m_pPropertiesDialog;

    int m_lastSortColumn = -1;
    bool m_lastSortAscending = true;
};

} // namespace pserv
