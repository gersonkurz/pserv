#pragma once
#include <core/data_controller.h>
#include <models/scheduled_task_info.h>

namespace pserv
{

    class ScheduledTasksDataController : public DataController
    {
    public:
        ScheduledTasksDataController();
        ~ScheduledTasksDataController() override = default;

        void Refresh() override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;
        VisualState GetVisualState(const DataObject *dataObject) const override;
    };

} // namespace pserv
