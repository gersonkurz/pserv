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
              {{"Name", "Name", ColumnDataType::String},
                  {"Status", "Status", ColumnDataType::String},
                  {"Trigger", "Trigger", ColumnDataType::String},
                  {"Last Run", "LastRun", ColumnDataType::String},
                  {"Next Run", "NextRun", ColumnDataType::String},
                  {"Author", "Author", ColumnDataType::String},
                  {"Enabled", "Enabled", ColumnDataType::String}}}
    {
    }

    void ScheduledTasksDataController::Refresh(bool isAutoRefresh)
    {
        spdlog::info("Refreshing scheduled tasks...");

        try
        {
            // Note: We don't call Clear() here - StartRefresh/FinishRefresh handles
            // update-in-place for existing objects and removes stale ones
            m_objects.StartRefresh();
            ScheduledTaskManager::EnumerateTasks(&m_objects);
            m_objects.FinishRefresh();

            // Re-apply last sort order if any
            if (m_lastSortColumn >= 0)
            {
                Sort(m_lastSortColumn, m_lastSortAscending);
            }

            spdlog::info("Successfully refreshed {} scheduled tasks", m_objects.GetSize());
            SetLoaded();
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to refresh scheduled tasks: {}", e.what());
        }
    }

    std::vector<const DataAction *> ScheduledTasksDataController::GetActions(const DataObject *dataObject) const
    {
        const auto *task = static_cast<const ScheduledTaskInfo *>(dataObject);
        return CreateScheduledTaskActions(task->GetState(), task->IsEnabled());
    }

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> ScheduledTasksDataController::GetAllActions() const
    {
        return CreateAllScheduledTaskActions();
    }
#endif

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
