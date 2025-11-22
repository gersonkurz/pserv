#pragma once
#include <core/data_action.h>
#include <models/environment_variable_info.h>

namespace pserv
{

    // Factory function to create environment variable actions
    std::vector<const DataAction *> CreateEnvironmentVariableActions(EnvironmentVariableScope scope);

} // namespace pserv
