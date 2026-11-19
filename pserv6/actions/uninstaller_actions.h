/// @file uninstaller_actions.h
/// @brief Actions for installed program management.
///
/// Factory functions for creating uninstaller-related actions like
/// uninstall, modify, and open registry location.
#pragma once

namespace pserv
{
    class DataAction;

    /// @brief Create actions for installed program management.
    /// @return Vector of uninstaller actions.
    ///
    /// Actions include: Uninstall, Modify, Open Registry Key.
    std::vector<const DataAction *> CreateUninstallerActions();

#ifdef PSERV_CONSOLE_BUILD
    /// @brief Create all possible uninstaller actions for CLI registration.
    std::vector<const DataAction *> CreateAllUninstallerActions();
#endif

} // namespace pserv
