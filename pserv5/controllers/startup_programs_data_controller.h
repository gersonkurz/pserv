#pragma once
#include <core/data_controller.h>
#include <models/startup_program_info.h>

namespace pserv
{

    class StartupProgramsDataController : public DataController
    {
    public:
        StartupProgramsDataController();
        ~StartupProgramsDataController() override = default;

        void Refresh() override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;
        VisualState GetVisualState(const DataObject *dataObject) const override;
    };

} // namespace pserv
