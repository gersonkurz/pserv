/// @file startup_program_actions.h
/// @brief Actions for startup program management.
///
/// Factory functions for creating startup-related actions like
/// enable, disable, delete, and open location.
#pragma once
#include <core/data_action.h>
#include <models/startup_program_info.h>

namespace pserv
{
    /// @brief Create actions appropriate for a startup entry's type and state.
    /// @param type Whether this is a registry or folder-based startup entry.
    /// @param enabled Whether the startup entry is currently enabled.
    /// @return Vector of applicable actions.
    ///
    /// Actions include: Enable, Disable, Delete, Open File Location,
    /// Open Registry Key (for registry entries).
    std::vector<const DataAction *> CreateStartupProgramActions(StartupProgramType type, bool enabled);

#ifdef PSERV_CONSOLE_BUILD
    /// @brief Create all possible startup program actions for CLI registration.
    std::vector<const DataAction *> CreateAllStartupProgramActions();
#endif

} // namespace pserv
