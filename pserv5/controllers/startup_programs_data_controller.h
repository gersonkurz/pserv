#pragma once
#include <core/data_controller.h>
#include <models/startup_program_info.h>

namespace pserv
{

    class StartupProgramsDataController : public DataController
    {
    public:
        StartupProgramsDataController();

    private:
        ~StartupProgramsDataController() override = default;

        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        VisualState GetVisualState(const DataObject *dataObject) const override;
    };

} // namespace pserv
