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
        void Refresh() override;
        VisualState GetVisualState(const DataObject *dataObject) const override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;
    };

} // namespace pserv
