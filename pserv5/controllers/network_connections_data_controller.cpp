#include "precomp.h"
#include <actions/network_connection_actions.h>
#include <controllers/network_connections_data_controller.h>
#include <models/network_connection_info.h>
#include <windows_api/network_connection_manager.h>

namespace pserv
{

    NetworkConnectionsDataController::NetworkConnectionsDataController()
        : DataController{NETWORK_CONNECTIONS_DATA_CONTROLLER_NAME,
              "Network Connection",
              {{"Protocol", "Protocol", ColumnDataType::String},
                  {"Local Address", "LocalAddress", ColumnDataType::String},
                  {"Local Port", "LocalPort", ColumnDataType::UnsignedInteger},
                  {"Remote Address", "RemoteAddress", ColumnDataType::String},
                  {"Remote Port", "RemotePort", ColumnDataType::UnsignedInteger},
                  {"State", "State", ColumnDataType::String},
                  {"PID", "ProcessId", ColumnDataType::UnsignedInteger},
                  {"Process Name", "ProcessName", ColumnDataType::String}}}
    {
    }

    void NetworkConnectionsDataController::Refresh()
    {
        spdlog::info("Refreshing network connections...");

        Clear();

        try
        {
            m_objects = NetworkConnectionManager::EnumerateConnections();

            // Re-apply last sort order if any
            if (m_lastSortColumn >= 0)
            {
                Sort(m_lastSortColumn, m_lastSortAscending);
            }

            spdlog::info("Successfully refreshed {} network connections", m_objects.size());
            m_bLoaded = true;
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to refresh network connections: {}", e.what());
        }
    }

    std::vector<const DataAction *> NetworkConnectionsDataController::GetActions(const DataObject *dataObject) const
    {
        const auto *connection = static_cast<const NetworkConnectionInfo *>(dataObject);
        return CreateNetworkConnectionActions(connection->GetProtocol(), connection->GetState());
    }

    VisualState NetworkConnectionsDataController::GetVisualState(const DataObject *dataObject) const
    {
        if (!dataObject)
        {
            return VisualState::Normal;
        }

        const auto *connection = static_cast<const NetworkConnectionInfo *>(dataObject);

        // Highlight established TCP connections
        if ((connection->GetProtocol() == NetworkProtocol::TCP || connection->GetProtocol() == NetworkProtocol::TCPv6) &&
            connection->GetState() == TcpState::Established)
        {
            return VisualState::Highlighted;
        }

        // Show listening ports as normal but could be customized
        if (connection->GetState() == TcpState::Listen)
        {
            return VisualState::Normal;
        }

        return VisualState::Normal;
    }

} // namespace pserv
