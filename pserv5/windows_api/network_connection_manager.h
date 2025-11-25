/// @file network_connection_manager.h
/// @brief Windows IP Helper API wrapper for network connections.
///
/// Provides enumeration of active TCP/UDP connections (IPv4 and IPv6)
/// and TCP connection termination using SetTcpEntry.
#pragma once
#include <core/data_object.h>
#include <models/network_connection_info.h>

namespace pserv
{
    class DataObjectContainer;

    /// @brief Static class for network connection enumeration and control.
    ///
    /// Uses GetExtendedTcpTable/GetExtendedUdpTable for enumeration
    /// and SetTcpEntry for closing TCP connections.
    class NetworkConnectionManager final
    {
    public:
        /// @brief Enumerate all network connections into a container.
        /// @param doc Container to populate with NetworkConnectionInfo objects.
        /// Enumerates TCP and UDP connections for both IPv4 and IPv6.
        static void EnumerateConnections(DataObjectContainer *doc);

        /// @brief Close a TCP connection.
        /// @param connection The TCP connection to close.
        /// @return true on success.
        /// @note Only works for TCP connections in Established state.
        /// @note Requires administrator privileges.
        static bool CloseConnection(const NetworkConnectionInfo *connection);

    private:
        static void EnumerateTcpConnections(DataObjectContainer *doc);
        static void EnumerateTcp6Connections(DataObjectContainer *doc);
        static void EnumerateUdpConnections(DataObjectContainer *doc);
        static void EnumerateUdp6Connections(DataObjectContainer *doc);
        static std::string GetProcessNameFromPid(DWORD pid);
        static std::string FormatIPv4Address(DWORD addr);
        static std::string FormatIPv6Address(const BYTE addr[16]);
    };

} // namespace pserv
