#pragma once
#include <core/data_controller.h>

namespace pserv
{

    class ModulesDataController : public DataController
    {
    public:
        ModulesDataController();

    private:
        void Refresh() override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;
        VisualState GetVisualState(const DataObject *dataObject) const override;
    };

} // namespace pserv
