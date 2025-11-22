#pragma once
#include <core/data_object.h>

namespace pserv
{

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

    enum class NetworkProtocol
    {
        TCP,
        UDP,
        TCPv6,
        UDPv6
    };

    // TCP connection states (from MIB_TCP_STATE)
    enum class TcpState
    {
        Closed = 1,
        Listen = 2,
        SynSent = 3,
        SynReceived = 4,
        Established = 5,
        FinWait1 = 6,
        FinWait2 = 7,
        CloseWait = 8,
        Closing = 9,
        LastAck = 10,
        TimeWait = 11,
        DeleteTcb = 12
    };

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
            DWORD localPort,
            std::string remoteAddress,
            DWORD remotePort,
            TcpState state,
            DWORD processId,
            std::string processName);
        ~NetworkConnectionInfo() override = default;

        // DataObject interface
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
