#pragma once
#include <core/data_action.h>
#include <models/network_connection_info.h>

namespace pserv
{

    // Factory function to create network connection actions
    std::vector<const DataAction *> CreateNetworkConnectionActions(NetworkProtocol protocol, TcpState state);

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> CreateAllNetworkConnectionActions();
#endif

} // namespace pserv
