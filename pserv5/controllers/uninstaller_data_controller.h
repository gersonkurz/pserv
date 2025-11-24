#pragma once
#include <core/data_controller.h>

namespace pserv
{

    class UninstallerDataController : public DataController
    {
    public:
        UninstallerDataController();

    private:
        void Refresh(bool isAutoRefresh = false) override;
        VisualState GetVisualState(const DataObject *dataObject) const override; // No special states for now
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif
    };

} // namespace pserv
