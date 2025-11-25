/// @file process_actions.h
/// @brief Actions for process management.
///
/// Factory functions for creating process-related actions like
/// terminate, open file location, and view modules.
#pragma once

namespace pserv
{
    class DataAction;

    /// @brief Create actions for process management.
    /// @return Vector of process actions.
    ///
    /// Actions include: Terminate, Open File Location, View Modules.
    std::vector<const DataAction *> CreateProcessActions();

#ifdef PSERV_CONSOLE_BUILD
    /// @brief Create all possible process actions for CLI registration.
    std::vector<const DataAction *> CreateAllProcessActions();
#endif

} // namespace pserv
