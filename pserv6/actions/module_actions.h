/// @file module_actions.h
/// @brief Actions for loaded module (DLL) inspection.
///
/// Factory functions for creating module-related actions like
/// open file location and copy path.
#pragma once

namespace pserv
{
    class DataAction;

    /// @brief Create actions for module inspection.
    /// @return Vector of module actions.
    ///
    /// Actions include: Open File Location, Copy Path.
    std::vector<const DataAction *> CreateModuleActions();

#ifdef PSERV_CONSOLE_BUILD
    /// @brief Create all possible module actions for CLI registration.
    std::vector<const DataAction *> CreateAllModuleActions();
#endif

} // namespace pserv
