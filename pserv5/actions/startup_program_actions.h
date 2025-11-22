#pragma once
#include <core/data_action.h>
#include <models/startup_program_info.h>

namespace pserv
{

    // Factory function to create startup program actions
    std::vector<const DataAction *> CreateStartupProgramActions(StartupProgramType type, bool enabled);

} // namespace pserv
