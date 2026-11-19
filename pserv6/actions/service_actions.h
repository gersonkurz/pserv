/// @file service_actions.h
/// @brief Actions for Windows service management.
///
/// Factory functions for creating service-related actions like
/// start, stop, pause, resume, and configuration changes.
#pragma once

namespace pserv
{
    class DataAction;

    /// @brief Create actions appropriate for a service's current state.
    /// @param currentState Service state (SERVICE_RUNNING, SERVICE_STOPPED, etc.).
    /// @param controlsAccepted Accepted control codes from SERVICE_STATUS.
    /// @return Vector of applicable actions for the service.
    ///
    /// Actions include: Start, Stop, Pause, Resume, Restart, Delete,
    /// and startup type changes (Auto, Manual, Disabled).
    std::vector<const DataAction *> CreateServiceActions(DWORD currentState, DWORD controlsAccepted);

#ifdef PSERV_CONSOLE_BUILD
    /// @brief Create all possible service actions for CLI registration.
    /// @return Complete action set regardless of service state.
    std::vector<const DataAction *> CreateAllServiceActions();
#endif

} // namespace pserv
