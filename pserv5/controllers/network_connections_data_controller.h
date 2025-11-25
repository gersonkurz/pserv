/// @file network_connections_data_controller.h
/// @brief Controller for active network connections.
///
/// Enumerates TCP and UDP connections using the IP Helper API,
/// similar to netstat output.
#pragma once
#include <core/data_controller.h>
#include <models/network_connection_info.h>

namespace pserv
{
    /// @brief Data controller for network connections.
    ///
    /// Uses GetExtendedTcpTable/GetExtendedUdpTable to enumerate active
    /// network connections, displaying:
    /// - Protocol (TCP/UDP) and state
    /// - Local and remote addresses/ports
    /// - Owning process ID and name
    ///
    /// Provides action to close TCP connections via SetTcpEntry API.
    class NetworkConnectionsDataController : public DataController
    {
    public:
        NetworkConnectionsDataController();
        ~NetworkConnectionsDataController() override = default;

        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

#ifdef PSERV_CONSOLE_BUILD
        std::vector<const DataAction *> GetAllActions() const override;
#endif

        VisualState GetVisualState(const DataObject *dataObject) const override;
    };

} // namespace pserv
