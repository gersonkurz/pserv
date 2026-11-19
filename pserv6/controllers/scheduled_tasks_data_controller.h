/// @file scheduled_tasks_data_controller.h
/// @brief Controller for Windows Task Scheduler tasks.
///
/// Enumerates scheduled tasks using the Task Scheduler COM API,
/// with support for running, enabling, and disabling tasks.
#pragma once
#include <core/data_controller.h>
#include <models/scheduled_task_info.h>

namespace pserv
{
    /// @brief Data controller for scheduled tasks.
    ///
    /// Uses the Task Scheduler 2.0 COM API to enumerate all scheduled
    /// tasks on the system, displaying:
    /// - Task name and path
    /// - Next/last run times
    /// - Task state and result
    /// - Triggers and actions summary
    ///
    /// @note Auto-refresh is disabled because task enumeration involves
    ///       recursive COM calls which can be slow on systems with many tasks.
    class ScheduledTasksDataController : public DataController
    {
    public:
        ScheduledTasksDataController();
        ~ScheduledTasksDataController() override = default;

        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        VisualState GetVisualState(const DataObject *dataObject) const override;
        bool SupportsAutoRefresh() const override { return false; }
    };

} // namespace pserv
