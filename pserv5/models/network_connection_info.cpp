#include "precomp.h"
#include <models/network_connection_info.h>
#include <utils/string_utils.h>

namespace pserv
{

    NetworkConnectionInfo::NetworkConnectionInfo(NetworkProtocol protocol,
        std::string localAddress,
        DWORD localPort,
        std::string remoteAddress,
        DWORD remotePort,
        TcpState state,
        DWORD processId,
        std::string processName)
        : m_protocol{protocol},
          m_localAddress{std::move(localAddress)},
          m_localPort{localPort},
          m_remoteAddress{std::move(remoteAddress)},
          m_remotePort{remotePort},
          m_state{state},
          m_processId{processId},
          m_processName{std::move(processName)}
    {
    }

    std::string NetworkConnectionInfo::GetProperty(int propertyId) const
    {
        switch (static_cast<NetworkConnectionProperty>(propertyId))
        {
        case NetworkConnectionProperty::Protocol:
            return GetProtocolString();
        case NetworkConnectionProperty::LocalAddress:
            return m_localAddress;
        case NetworkConnectionProperty::LocalPort:
            return std::to_string(m_localPort);
        case NetworkConnectionProperty::RemoteAddress:
            return m_remoteAddress;
        case NetworkConnectionProperty::RemotePort:
            return std::to_string(m_remotePort);
        case NetworkConnectionProperty::State:
            return GetStateString();
        case NetworkConnectionProperty::ProcessId:
            return std::to_string(m_processId);
        case NetworkConnectionProperty::ProcessName:
            return m_processName;
        default:
            return "";
        }
    }

    PropertyValue NetworkConnectionInfo::GetTypedProperty(int propertyId) const
    {
        switch (static_cast<NetworkConnectionProperty>(propertyId))
        {
        case NetworkConnectionProperty::LocalPort:
        case NetworkConnectionProperty::RemotePort:
        case NetworkConnectionProperty::ProcessId:
            return PropertyValue{static_cast<uint64_t>(std::stoull(GetProperty(propertyId)))};
        default:
            return PropertyValue{GetProperty(propertyId)};
        }
    }

    bool NetworkConnectionInfo::MatchesFilter(const std::string &filter) const
    {
        if (filter.empty())
            return true;

        std::string lowerFilter = utils::ToLower(filter);
        return utils::ToLower(GetProtocolString()).find(lowerFilter) != std::string::npos ||
               utils::ToLower(m_localAddress).find(lowerFilter) != std::string::npos ||
               utils::ToLower(m_remoteAddress).find(lowerFilter) != std::string::npos ||
               utils::ToLower(GetStateString()).find(lowerFilter) != std::string::npos ||
               utils::ToLower(m_processName).find(lowerFilter) != std::string::npos ||
               std::to_string(m_localPort).find(filter) != std::string::npos ||
               std::to_string(m_remotePort).find(filter) != std::string::npos ||
               std::to_string(m_processId).find(filter) != std::string::npos;
    }

    std::string NetworkConnectionInfo::GetProtocolString() const
    {
        switch (m_protocol)
        {
        case NetworkProtocol::TCP:
            return "TCP";
        case NetworkProtocol::UDP:
            return "UDP";
        case NetworkProtocol::TCPv6:
            return "TCPv6";
        case NetworkProtocol::UDPv6:
            return "UDPv6";
        default:
            return "Unknown";
        }
    }

    std::string NetworkConnectionInfo::GetStateString() const
    {
        // Only TCP has states
        if (m_protocol == NetworkProtocol::UDP || m_protocol == NetworkProtocol::UDPv6)
        {
            return "";
        }

        switch (m_state)
        {
        case TcpState::Closed:
            return "CLOSED";
        case TcpState::Listen:
            return "LISTENING";
        case TcpState::SynSent:
            return "SYN_SENT";
        case TcpState::SynReceived:
            return "SYN_RECEIVED";
        case TcpState::Established:
            return "ESTABLISHED";
        case TcpState::FinWait1:
            return "FIN_WAIT1";
        case TcpState::FinWait2:
            return "FIN_WAIT2";
        case TcpState::CloseWait:
            return "CLOSE_WAIT";
        case TcpState::Closing:
            return "CLOSING";
        case TcpState::LastAck:
            return "LAST_ACK";
        case TcpState::TimeWait:
            return "TIME_WAIT";
        case TcpState::DeleteTcb:
            return "DELETE_TCB";
        default:
            return "UNKNOWN";
        }
    }

    std::string NetworkConnectionInfo::GetLocalEndpoint() const
    {
        return std::format("{}:{}", m_localAddress, m_localPort);
    }

    std::string NetworkConnectionInfo::GetRemoteEndpoint() const
    {
        return std::format("{}:{}", m_remoteAddress, m_remotePort);
    }

} // namespace pserv
