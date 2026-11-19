/// @file window_actions.h
/// @brief Actions for window manipulation.
///
/// Factory functions for creating window-related actions like
/// show, hide, minimize, maximize, and close.
#pragma once

namespace pserv
{
    class DataAction;

    /// @brief Create actions for window manipulation.
    /// @return Vector of window actions.
    ///
    /// Actions include: Show, Hide, Minimize, Maximize, Restore, Close,
    /// Bring to Front, Send to Back.
    std::vector<const DataAction *> CreateWindowActions();

#ifdef PSERV_CONSOLE_BUILD
    /// @brief Create all possible window actions for CLI registration.
    std::vector<const DataAction *> CreateAllWindowActions();
#endif

} // namespace pserv
