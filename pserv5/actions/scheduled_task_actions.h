#pragma once
#include <core/data_action.h>
#include <models/scheduled_task_info.h>

namespace pserv
{

    // Factory function to create scheduled task actions
    std::vector<const DataAction *> CreateScheduledTaskActions(ScheduledTaskState state, bool enabled);

} // namespace pserv
