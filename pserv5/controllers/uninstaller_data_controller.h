#pragma once
#include <core/data_controller.h>
#include <models/installed_program_info.h>

namespace pserv {

class UninstallerDataController : public DataController {
public:
    UninstallerDataController();

private:
    void Refresh() override;
    VisualState GetVisualState(const DataObject* dataObject) const override; // No special states for now
    std::vector<const DataAction*> GetActions(const DataObject* dataObject) const override;
};

} // namespace pserv
