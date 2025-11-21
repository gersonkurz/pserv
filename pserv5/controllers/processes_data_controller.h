#pragma once
#include <core/data_controller.h>
#include <models/process_info.h>

namespace pserv
{

    class ProcessesDataController : public DataController
    {
    public:
        ProcessesDataController();

    private:
        // DataController interface
        void Refresh() override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;
        VisualState GetVisualState(const DataObject *dataObject) const override;

    private:
        std::string m_currentUserName;
    };

} // namespace pserv
