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

        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        VisualState GetVisualState(const DataObject *dataObject) const override;
        bool SupportsAutoRefresh() const override
        {
            return false; // Enumeration is slow (recursive COM calls)
        }
    };

} // namespace pserv
