#pragma once
#include <core/data_controller.h>

namespace pserv
{

    class WindowsDataController : public DataController
    {
    public:
        WindowsDataController();

    private:
        // Base class overrides
        void Refresh(bool isAutoRefresh = false) override;
        VisualState GetVisualState(const DataObject *dataObject) const override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        bool SupportsAutoRefresh() const override
        {
            return false; // Windows list too volatile/distracting
        }
    };

} // namespace pserv
