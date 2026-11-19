#include "precomp.h"
#include <actions/uninstaller_actions.h>
#include <controllers/uninstaller_data_controller.h>
#include <models/installed_program_info.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/uninstaller_manager.h>

namespace pserv
{

    UninstallerDataController::UninstallerDataController()
        : DataController{UNINSTALLER_DATA_CONTROLLER_NAME,
              "Program",
              {{ProgramProperty::DisplayName, "Display Name", "DisplayName", ColumnDataType::String},
                  {ProgramProperty::Version, "Version", "Version", ColumnDataType::String},
                  {ProgramProperty::Publisher, "Publisher", "Publisher", ColumnDataType::String},
                  {ProgramProperty::InstallLocation, "Install Location", "InstallLocation", ColumnDataType::String},
                  {ProgramProperty::UninstallString, "Uninstall String", "UninstallString", ColumnDataType::String},
                  {ProgramProperty::InstallDate, "Install Date", "InstallDate", ColumnDataType::String},
                  {ProgramProperty::EstimatedSize, "Estimated Size", "EstimatedSize", ColumnDataType::Size},
                  {ProgramProperty::Comments, "Comments", "Comments", ColumnDataType::String},
                  {ProgramProperty::HelpLink, "Help Link", "HelpLink", ColumnDataType::String},
                  {ProgramProperty::URLInfoAbout, "URL Info About", "URLInfoAbout", ColumnDataType::String}}}
    {
    }

    void UninstallerDataController::Collect(DataObjectContainer &target, const RefreshRequest &request)
    {
        target.StartRefresh();
        UninstallerManager::EnumerateInstalledPrograms(&target);
        target.FinishRefresh();
    }

    std::vector<const DataAction *> UninstallerDataController::GetActions(const DataObject *dataObject) const
    {
        return CreateUninstallerActions();
    }

    std::vector<const DataAction *> UninstallerDataController::GetAllActions() const
    {
        return CreateAllUninstallerActions();
    }

    VisualState UninstallerDataController::GetVisualState(const DataObject *dataObject) const
    {
        // No special visual states for installed programs currently
        return VisualState::Normal;
    }
} // namespace pserv