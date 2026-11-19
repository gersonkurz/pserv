/// @file environment_variable_actions.h
/// @brief Actions for environment variable management.
///
/// Factory functions for creating environment variable actions like
/// add, edit, delete, and copy.
#pragma once
#include <core/data_action.h>
#include <models/environment_variable_info.h>

namespace pserv
{
    /// @brief Create actions for environment variable management.
    /// @param scope User or System scope for the variable.
    /// @return Vector of applicable actions.
    ///
    /// Actions include: Add User Variable, Add System Variable,
    /// Delete, Copy Name, Copy Value.
    std::vector<const DataAction *> CreateEnvironmentVariableActions(EnvironmentVariableScope scope);

#ifdef PSERV_CONSOLE_BUILD
    /// @brief Create all possible environment variable actions for CLI registration.
    std::vector<const DataAction *> CreateAllEnvironmentVariableActions();
#endif

} // namespace pserv
