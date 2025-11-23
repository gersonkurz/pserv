#pragma once
#include <core/data_controller.h>

namespace pserv
{

    class ModulesDataController : public DataController
    {
    public:
        ModulesDataController();

    private:
        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;
        VisualState GetVisualState(const DataObject *dataObject) const override;
        bool SupportsAutoRefresh() const override
        {
            return false; // Modules are context-specific to a process
        }
    };

} // namespace pserv
