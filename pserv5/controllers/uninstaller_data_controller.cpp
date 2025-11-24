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
              {{"Display Name", "DisplayName", ColumnDataType::String},
                  {"Version", "Version", ColumnDataType::String},
                  {"Publisher", "Publisher", ColumnDataType::String},
                  {"Install Location", "InstallLocation", ColumnDataType::String},
                  {"Uninstall String", "UninstallString", ColumnDataType::String},
                  {"Install Date", "InstallDate", ColumnDataType::String},
                  {"Estimated Size", "EstimatedSize", ColumnDataType::Size},
                  {"Comments", "Comments", ColumnDataType::String},
                  {"Help Link", "HelpLink", ColumnDataType::String},
                  {"URL Info About", "URLInfoAbout", ColumnDataType::String}}}
    {
    }

    void UninstallerDataController::Refresh(bool isAutoRefresh)
    {
        spdlog::info("Refreshing installed programs...");

        // Note: We don't call Clear() here - StartRefresh/FinishRefresh handles
        // update-in-place for existing objects and removes stale ones
        m_objects.StartRefresh();
        UninstallerManager::EnumerateInstalledPrograms(&m_objects);
        m_objects.FinishRefresh();

        // Re-apply last sort order if any
        if (m_lastSortColumn >= 0)
        {
            Sort(m_lastSortColumn, m_lastSortAscending);
        }

        spdlog::info("Refreshed {} installed programs", m_objects.GetSize());
        m_bLoaded = true;
    }

    std::vector<const DataAction *> UninstallerDataController::GetActions(const DataObject *dataObject) const
    {
        return CreateUninstallerActions();
    }

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> UninstallerDataController::GetAllActions() const
    {
        return CreateAllUninstallerActions();
    }
#endif

    VisualState UninstallerDataController::GetVisualState(const DataObject *dataObject) const
    {
        // No special visual states for installed programs currently
        return VisualState::Normal;
    }
} // namespace pserv