/// @file scheduled_task_actions.h
/// @brief Actions for Windows Task Scheduler task management.
///
/// Factory functions for creating task-related actions like
/// run, enable, disable, and end.
#pragma once
#include <core/data_action.h>
#include <models/scheduled_task_info.h>

namespace pserv
{
    /// @brief Create actions appropriate for a task's current state.
    /// @param state Task execution state (Running, Ready, Disabled, etc.).
    /// @param enabled Whether the task is enabled.
    /// @return Vector of applicable actions.
    ///
    /// Actions include: Run, End (if running), Enable, Disable.
    std::vector<const DataAction *> CreateScheduledTaskActions(ScheduledTaskState state, bool enabled);

#ifdef PSERV_CONSOLE_BUILD
    /// @brief Create all possible scheduled task actions for CLI registration.
    std::vector<const DataAction *> CreateAllScheduledTaskActions();
#endif

} // namespace pserv
