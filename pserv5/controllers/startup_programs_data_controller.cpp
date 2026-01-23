#include "precomp.h"
#include <actions/startup_program_actions.h>
#include <controllers/startup_programs_data_controller.h>
#include <models/startup_program_info.h>
#include <windows_api/startup_program_manager.h>

namespace pserv
{

    StartupProgramsDataController::StartupProgramsDataController()
        : DataController{STARTUP_PROGRAMS_DATA_CONTROLLER_NAME,
              "Startup Program",
              {{"Name", "Name", ColumnDataType::String},
                  {"Command", "Command", ColumnDataType::String},
                  {"Location", "Location", ColumnDataType::String},
                  {"Type", "Type", ColumnDataType::String},
                  {"Enabled", "Enabled", ColumnDataType::String}}}
    {
    }

    void StartupProgramsDataController::Refresh(bool isAutoRefresh)
    {
        spdlog::info("Refreshing startup programs...");

        try
        {
            // Note: We don't call Clear() here - StartRefresh/FinishRefresh handles
            // update-in-place for existing objects and removes stale ones
            m_objects.StartRefresh();
            StartupProgramManager::EnumerateStartupPrograms(&m_objects);
            m_objects.FinishRefresh();

            // Re-apply last sort order if any
            if (m_lastSortColumn >= 0)
            {
                Sort(m_lastSortColumn, m_lastSortAscending);
            }

            spdlog::info("Successfully refreshed {} startup programs", m_objects.GetSize());
            SetLoaded();
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to refresh startup programs: {}", e.what());
        }
    }

    std::vector<const DataAction *> StartupProgramsDataController::GetActions(const DataObject *dataObject) const
    {
        const auto *program = static_cast<const StartupProgramInfo *>(dataObject);
        return CreateStartupProgramActions(program->GetType(), program->IsEnabled());
    }

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> StartupProgramsDataController::GetAllActions() const
    {
        return CreateAllStartupProgramActions();
    }
#endif

    VisualState StartupProgramsDataController::GetVisualState(const DataObject *dataObject) const
    {
        if (!dataObject)
        {
            return VisualState::Normal;
        }

        const auto *program = static_cast<const StartupProgramInfo *>(dataObject);

        // Disabled programs shown as disabled
        if (!program->IsEnabled())
        {
            return VisualState::Disabled;
        }

        // System scope programs highlighted
        if (program->GetScope() == StartupProgramScope::System)
        {
            return VisualState::Highlighted;
        }

        return VisualState::Normal;
    }

} // namespace pserv
