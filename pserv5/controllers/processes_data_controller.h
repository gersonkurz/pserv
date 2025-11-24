#pragma once
#include <core/data_controller.h>

namespace pserv
{
    class ProcessesDataController : public DataController
    {
    public:
        ProcessesDataController();

    private:
        // DataController interface
        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        VisualState GetVisualState(const DataObject *dataObject) const override;

    private:
        std::string m_currentUserName;
    };

} // namespace pserv
