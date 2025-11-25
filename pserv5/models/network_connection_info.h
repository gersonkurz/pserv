/// @file network_connection_info.h
/// @brief Data model for active network connection information.
///
/// Contains NetworkConnectionInfo class representing a TCP or UDP
/// connection/socket from the IP Helper API.
#pragma once
#include <core/data_object.h>

namespace pserv
{
    /// @brief Column indices for network connection properties.
    enum class NetworkConnectionProperty
    {
        Protocol = 0,
        LocalAddress,
        LocalPort,
        RemoteAddress,
        RemotePort,
        State,
        ProcessId,
        ProcessName
    };

    /// @brief Network protocol type.
    enum class NetworkProtocol
    {
        TCP,    ///< TCP over IPv4.
        UDP,    ///< UDP over IPv4.
        TCPv6,  ///< TCP over IPv6.
        UDPv6   ///< UDP over IPv6.
    };

    /// @brief TCP connection state (matches MIB_TCP_STATE).
    enum class TcpState
    {
        Closed = 1,       ///< Connection closed.
        Listen = 2,       ///< Listening for connections.
        SynSent = 3,      ///< SYN packet sent.
        SynReceived = 4,  ///< SYN packet received.
        Established = 5,  ///< Connection established.
        FinWait1 = 6,     ///< FIN sent, waiting for ACK.
        FinWait2 = 7,     ///< FIN acknowledged, waiting for FIN.
        CloseWait = 8,    ///< FIN received, waiting for close.
        Closing = 9,      ///< Both sides sent FIN.
        LastAck = 10,     ///< Waiting for final ACK.
        TimeWait = 11,    ///< Waiting for delayed packets.
        DeleteTcb = 12    ///< TCB being deleted.
    };

    /// @brief Data model representing a network connection.
    ///
    /// Stores connection information from IP Helper API:
    /// - Endpoints: local/remote address and port
    /// - Protocol: TCP, UDP, IPv4, IPv6
    /// - State: TCP connection state
    /// - Owner: process ID and name
    class NetworkConnectionInfo : public DataObject
    {
    private:
        NetworkProtocol m_protocol;
        std::string m_localAddress;
        DWORD m_localPort;
        std::string m_remoteAddress;
        DWORD m_remotePort;
        TcpState m_state; // Only valid for TCP
        DWORD m_processId;
        std::string m_processName;

    public:
        NetworkConnectionInfo(NetworkProtocol protocol,
            std::string localAddress,
            DWORD localPort);
        void SetValues(
            std::string remoteAddress,
            DWORD remotePort,
            TcpState state,
            DWORD processId,
            std::string processName);
        ~NetworkConnectionInfo() override = default;

        // DataObject interface
        static std::string GetStableID(NetworkProtocol protocol, const std::string& localAddress, DWORD localPort)
        {
            return std::format("{}:{}:{}", static_cast<int>(protocol), localAddress, std::to_string(localPort));
        }

        std::string GetStableID() const
        {
            return GetStableID(m_protocol, m_localAddress, m_localPort);
        }

        std::string GetProperty(int propertyId) const override;
        PropertyValue GetTypedProperty(int propertyId) const override;
        bool MatchesFilter(const std::string &filter) const override;
        std::string GetItemName() const
        {
            return GetProperty(static_cast<int>(NetworkConnectionProperty::Protocol));
        }

        // Getters
        NetworkProtocol GetProtocol() const { return m_protocol; }
        const std::string &GetLocalAddress() const { return m_localAddress; }
        DWORD GetLocalPort() const { return m_localPort; }
        const std::string &GetRemoteAddress() const { return m_remoteAddress; }
        DWORD GetRemotePort() const { return m_remotePort; }
        TcpState GetState() const { return m_state; }
        DWORD GetProcessId() const { return m_processId; }
        const std::string &GetProcessName() const { return m_processName; }

        std::string GetProtocolString() const;
        std::string GetStateString() const;
        std::string GetLocalEndpoint() const;
        std::string GetRemoteEndpoint() const;
    };

} // namespace pserv
