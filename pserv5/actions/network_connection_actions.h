/// @file network_connection_actions.h
/// @brief Actions for network connection management.
///
/// Factory functions for creating connection-related actions like
/// close TCP connection.
#pragma once
#include <core/data_action.h>
#include <models/network_connection_info.h>

namespace pserv
{
    /// @brief Create actions appropriate for a connection's protocol and state.
    /// @param protocol TCP or UDP.
    /// @param state TCP connection state (Established, Listen, etc.).
    /// @return Vector of applicable actions.
    ///
    /// Actions include: Close Connection (TCP only, established connections).
    std::vector<const DataAction *> CreateNetworkConnectionActions(NetworkProtocol protocol, TcpState state);

#ifdef PSERV_CONSOLE_BUILD
    /// @brief Create all possible network connection actions for CLI registration.
    std::vector<const DataAction *> CreateAllNetworkConnectionActions();
#endif

} // namespace pserv
