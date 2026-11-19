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
              {{StartupProgramProperty::Name, "Name", "Name", ColumnDataType::String},
                  {StartupProgramProperty::Command, "Command", "Command", ColumnDataType::String},
                  {StartupProgramProperty::Location, "Location", "Location", ColumnDataType::String},
                  {StartupProgramProperty::Type, "Type", "Type", ColumnDataType::String},
                  {StartupProgramProperty::Enabled, "Enabled", "Enabled", ColumnDataType::String}}}
    {
    }

    void StartupProgramsDataController::Collect(DataObjectContainer &target, const RefreshRequest &request)
    {
        target.StartRefresh();
        StartupProgramManager::EnumerateStartupPrograms(&target);
        target.FinishRefresh();
    }

    std::vector<const DataAction *> StartupProgramsDataController::GetActions(const DataObject *dataObject) const
    {
        const auto *program = static_cast<const StartupProgramInfo *>(dataObject);
        return CreateStartupProgramActions(program->GetType(), program->IsEnabled());
    }

    std::vector<const DataAction *> StartupProgramsDataController::GetAllActions() const
    {
        return CreateAllStartupProgramActions();
    }

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
