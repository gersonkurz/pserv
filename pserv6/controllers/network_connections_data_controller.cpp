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
              {{NetworkConnectionProperty::Protocol, "Protocol", "Protocol", ColumnDataType::String},
                  {NetworkConnectionProperty::LocalAddress, "Local Address", "LocalAddress", ColumnDataType::String},
                  {NetworkConnectionProperty::LocalPort, "Local Port", "LocalPort", ColumnDataType::UnsignedInteger},
                  {NetworkConnectionProperty::RemoteAddress, "Remote Address", "RemoteAddress", ColumnDataType::String},
                  {NetworkConnectionProperty::RemotePort, "Remote Port", "RemotePort", ColumnDataType::UnsignedInteger},
                  {NetworkConnectionProperty::State, "State", "State", ColumnDataType::String},
                  {NetworkConnectionProperty::ProcessId, "PID", "ProcessId", ColumnDataType::UnsignedInteger},
                  {NetworkConnectionProperty::ProcessName, "Process Name", "ProcessName", ColumnDataType::String}}}
    {
    }

    void NetworkConnectionsDataController::Collect(DataObjectContainer &target, const RefreshRequest &request)
    {
        target.StartRefresh();
        NetworkConnectionManager::EnumerateConnections(&target);
        target.FinishRefresh();
    }

    std::vector<const DataAction *> NetworkConnectionsDataController::GetActions(const DataObject *dataObject) const
    {
        const auto *connection = static_cast<const NetworkConnectionInfo *>(dataObject);
        return CreateNetworkConnectionActions(connection->GetProtocol(), connection->GetState());
    }

    std::vector<const DataAction *> NetworkConnectionsDataController::GetAllActions() const
    {
        return CreateAllNetworkConnectionActions();
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
