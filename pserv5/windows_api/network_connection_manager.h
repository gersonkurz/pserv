#pragma once
#include <core/data_object.h>
#include <models/network_connection_info.h>

namespace pserv
{
    class DataObjectContainer;

    class NetworkConnectionManager final
    {
    public:
        // Enumerate all network connections
        static void EnumerateConnections(DataObjectContainer *doc);

        // Close a TCP connection
        static bool CloseConnection(const NetworkConnectionInfo *connection);

    private:
        // Helper to enumerate TCP connections (IPv4)
        static void EnumerateTcpConnections(DataObjectContainer *doc);

        // Helper to enumerate TCP connections (IPv6)
        static void EnumerateTcp6Connections(DataObjectContainer *doc);

        // Helper to enumerate UDP connections (IPv4)
        static void EnumerateUdpConnections(DataObjectContainer *doc);

        // Helper to enumerate UDP connections (IPv6)
        static void EnumerateUdp6Connections(DataObjectContainer *doc);

        // Helper to get process name from PID
        static std::string GetProcessNameFromPid(DWORD pid);

        // Helper to format IPv4 address
        static std::string FormatIPv4Address(DWORD addr);

        // Helper to format IPv6 address
        static std::string FormatIPv6Address(const BYTE addr[16]);
    };

} // namespace pserv
