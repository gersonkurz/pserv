#include "precomp.h"
#include <actions/scheduled_task_actions.h>
#include <controllers/scheduled_tasks_data_controller.h>
#include <models/scheduled_task_info.h>
#include <windows_api/scheduled_task_manager.h>

namespace pserv
{

    ScheduledTasksDataController::ScheduledTasksDataController()
        : DataController{SCHEDULED_TASKS_DATA_CONTROLLER_NAME,
              "Scheduled Task",
              {{ScheduledTaskProperty::Name, "Name", "Name", ColumnDataType::String},
                  {ScheduledTaskProperty::Status, "Status", "Status", ColumnDataType::String},
                  {ScheduledTaskProperty::Trigger, "Trigger", "Trigger", ColumnDataType::String},
                  {ScheduledTaskProperty::LastRun, "Last Run", "LastRun", ColumnDataType::String},
                  {ScheduledTaskProperty::NextRun, "Next Run", "NextRun", ColumnDataType::String},
                  {ScheduledTaskProperty::Author, "Author", "Author", ColumnDataType::String},
                  {ScheduledTaskProperty::Enabled, "Enabled", "Enabled", ColumnDataType::String}}}
    {
    }

    void ScheduledTasksDataController::Collect(DataObjectContainer &target, const RefreshRequest &request)
    {
        target.StartRefresh();
        ScheduledTaskManager::EnumerateTasks(&target);
        target.FinishRefresh();
    }

    std::vector<const DataAction *> ScheduledTasksDataController::GetActions(const DataObject *dataObject) const
    {
        const auto *task = static_cast<const ScheduledTaskInfo *>(dataObject);
        return CreateScheduledTaskActions(task->GetState(), task->IsEnabled());
    }

    std::vector<const DataAction *> ScheduledTasksDataController::GetAllActions() const
    {
        return CreateAllScheduledTaskActions();
    }

    VisualState ScheduledTasksDataController::GetVisualState(const DataObject *dataObject) const
    {
        if (!dataObject)
        {
            return VisualState::Normal;
        }

        const auto *task = static_cast<const ScheduledTaskInfo *>(dataObject);

        // Running tasks highlighted
        if (task->GetState() == ScheduledTaskState::Running)
        {
            return VisualState::Highlighted;
        }

        // Disabled tasks grayed out
        if (task->GetState() == ScheduledTaskState::Disabled || !task->IsEnabled())
        {
            return VisualState::Disabled;
        }

        return VisualState::Normal;
    }

} // namespace pserv
