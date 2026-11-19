#include "precomp.h"
#include <core/data_object_container.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/network_connection_manager.h>
#include <ws2tcpip.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// Define IPv6 structures if not available in SDK
#ifndef MIB_TCP6ROW_OWNER_PID
typedef struct _MIB_TCP6ROW_OWNER_PID
{
    UCHAR ucLocalAddr[16];
    DWORD dwLocalScopeId;
    DWORD dwLocalPort;
    UCHAR ucRemoteAddr[16];
    DWORD dwRemoteScopeId;
    DWORD dwRemotePort;
    DWORD dwState;
    DWORD dwOwningPid;
} MIB_TCP6ROW_OWNER_PID, *PMIB_TCP6ROW_OWNER_PID;

typedef struct _MIB_TCP6TABLE_OWNER_PID
{
    DWORD dwNumEntries;
    MIB_TCP6ROW_OWNER_PID table[1];
} MIB_TCP6TABLE_OWNER_PID, *PMIB_TCP6TABLE_OWNER_PID;
#endif

#ifndef MIB_UDP6ROW_OWNER_PID
typedef struct _MIB_UDP6ROW_OWNER_PID
{
    UCHAR ucLocalAddr[16];
    DWORD dwLocalScopeId;
    DWORD dwLocalPort;
    DWORD dwOwningPid;
} MIB_UDP6ROW_OWNER_PID, *PMIB_UDP6ROW_OWNER_PID;

typedef struct _MIB_UDP6TABLE_OWNER_PID
{
    DWORD dwNumEntries;
    MIB_UDP6ROW_OWNER_PID table[1];
} MIB_UDP6TABLE_OWNER_PID, *PMIB_UDP6TABLE_OWNER_PID;
#endif

namespace pserv
{

    void NetworkConnectionManager::EnumerateConnections(DataObjectContainer *doc)
    {
        EnumerateTcpConnections(doc);
        EnumerateTcp6Connections(doc);
        EnumerateUdpConnections(doc);
        EnumerateUdp6Connections(doc);
    }

    void NetworkConnectionManager::EnumerateTcpConnections(DataObjectContainer *doc)
    {
        DWORD size = 0;
        DWORD result = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

        if (result != ERROR_INSUFFICIENT_BUFFER)
        {
            LogWin32ErrorCode("GetExtendedTcpTable", result, "querying TCP table size");
            return;
        }

        std::vector<BYTE> buffer(size);
        auto *pTcpTable = reinterpret_cast<MIB_TCPTABLE_OWNER_PID *>(buffer.data());

        result = GetExtendedTcpTable(pTcpTable, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        if (result != NO_ERROR)
        {
            LogWin32ErrorCode("GetExtendedTcpTable", result, "enumerating TCP connections");
            return;
        }

        for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++)
        {
            const auto &row = pTcpTable->table[i];

            std::string localAddr = FormatIPv4Address(row.dwLocalAddr);
            DWORD localPort = ntohs((u_short)row.dwLocalPort);

            std::string remoteAddr = FormatIPv4Address(row.dwRemoteAddr);
            DWORD remotePort = ntohs((u_short)row.dwRemotePort);

            TcpState state = static_cast<TcpState>(row.dwState);
            DWORD pid = row.dwOwningPid;
            std::string processName = GetProcessNameFromPid(pid);

            const auto protocol = NetworkProtocol::TCP;
            const auto stableId{NetworkConnectionInfo::GetStableID(protocol, localAddr, localPort)};
            auto nci = doc->GetByStableId<NetworkConnectionInfo>(stableId);
            if (nci == nullptr)
            {
                nci = doc->Append<NetworkConnectionInfo>(DBG_NEW NetworkConnectionInfo{protocol, localAddr, localPort});
            }
            nci->SetValues(remoteAddr, remotePort, state, pid, processName);
        }
    }

    void NetworkConnectionManager::EnumerateTcp6Connections(DataObjectContainer *doc)
    {
        DWORD size = 0;
        DWORD result = GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);

        if (result != ERROR_INSUFFICIENT_BUFFER)
        {
            LogWin32ErrorCode("GetExtendedTcpTable", result, "querying TCPv6 table size");
            return;
        }

        std::vector<BYTE> buffer(size);
        auto *pTcp6Table = reinterpret_cast<MIB_TCP6TABLE_OWNER_PID *>(buffer.data());

        result = GetExtendedTcpTable(pTcp6Table, &size, FALSE, AF_INET6, TCP_TABLE_OWNER_PID_ALL, 0);
        if (result != NO_ERROR)
        {
            LogWin32ErrorCode("GetExtendedTcpTable", result, "enumerating TCPv6 connections");
            return;
        }

        for (DWORD i = 0; i < pTcp6Table->dwNumEntries; i++)
        {
            const auto &row = pTcp6Table->table[i];

            std::string localAddr = FormatIPv6Address(row.ucLocalAddr);
            DWORD localPort = ntohs((u_short)row.dwLocalPort);

            std::string remoteAddr = FormatIPv6Address(row.ucRemoteAddr);
            DWORD remotePort = ntohs((u_short)row.dwRemotePort);

            TcpState state = static_cast<TcpState>(row.dwState);
            DWORD pid = row.dwOwningPid;
            std::string processName = GetProcessNameFromPid(pid);

            const auto protocol = NetworkProtocol::TCPv6;
            const auto stableId{NetworkConnectionInfo::GetStableID(protocol, localAddr, localPort)};
            auto nci = doc->GetByStableId<NetworkConnectionInfo>(stableId);
            if (nci == nullptr)
            {
                nci = doc->Append<NetworkConnectionInfo>(DBG_NEW NetworkConnectionInfo{protocol, localAddr, localPort});
            }
            nci->SetValues(remoteAddr, remotePort, state, pid, processName);
        }
    }

    void NetworkConnectionManager::EnumerateUdpConnections(DataObjectContainer *doc)
    {
        DWORD size = 0;
        DWORD result = GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);

        if (result != ERROR_INSUFFICIENT_BUFFER)
        {
            LogWin32ErrorCode("GetExtendedUdpTable", result, "querying UDP table size");
            return;
        }

        std::vector<BYTE> buffer(size);
        auto *pUdpTable = reinterpret_cast<MIB_UDPTABLE_OWNER_PID *>(buffer.data());

        result = GetExtendedUdpTable(pUdpTable, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
        if (result != NO_ERROR)
        {
            LogWin32ErrorCode("GetExtendedUdpTable", result, "enumerating UDP connections");
            return;
        }

        for (DWORD i = 0; i < pUdpTable->dwNumEntries; i++)
        {
            const auto &row = pUdpTable->table[i];

            std::string localAddr = FormatIPv4Address(row.dwLocalAddr);
            DWORD localPort = ntohs((u_short)row.dwLocalPort);

            DWORD pid = row.dwOwningPid;
            std::string processName = GetProcessNameFromPid(pid);

            // UDP has no remote endpoint or state
            const auto protocol = NetworkProtocol::UDP;
            const auto stableId{NetworkConnectionInfo::GetStableID(protocol, localAddr, localPort)};
            auto nci = doc->GetByStableId<NetworkConnectionInfo>(stableId);
            if (nci == nullptr)
            {
                nci = doc->Append<NetworkConnectionInfo>(DBG_NEW NetworkConnectionInfo{protocol, localAddr, localPort});
            }
            nci->SetValues("*", 0, TcpState::Closed, pid, processName);
        }
    }

    void NetworkConnectionManager::EnumerateUdp6Connections(DataObjectContainer *doc)
    {
        DWORD size = 0;
        DWORD result = GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);

        if (result != ERROR_INSUFFICIENT_BUFFER)
        {
            LogWin32ErrorCode("GetExtendedUdpTable", result, "querying UDPv6 table size");
            return;
        }

        std::vector<BYTE> buffer(size);
        auto *pUdp6Table = reinterpret_cast<MIB_UDP6TABLE_OWNER_PID *>(buffer.data());

        result = GetExtendedUdpTable(pUdp6Table, &size, FALSE, AF_INET6, UDP_TABLE_OWNER_PID, 0);
        if (result != NO_ERROR)
        {
            LogWin32ErrorCode("GetExtendedUdpTable", result, "enumerating UDPv6 connections");
            return;
        }

        for (DWORD i = 0; i < pUdp6Table->dwNumEntries; i++)
        {
            const auto &row = pUdp6Table->table[i];

            std::string localAddr = FormatIPv6Address(row.ucLocalAddr);
            DWORD localPort = ntohs((u_short)row.dwLocalPort);

            DWORD pid = row.dwOwningPid;
            std::string processName = GetProcessNameFromPid(pid);

            const auto protocol = NetworkProtocol::UDPv6;
            const auto stableId{NetworkConnectionInfo::GetStableID(protocol, localAddr, localPort)};
            auto nci = doc->GetByStableId<NetworkConnectionInfo>(stableId);
            if (nci == nullptr)
            {
                nci = doc->Append<NetworkConnectionInfo>(DBG_NEW NetworkConnectionInfo{protocol, localAddr, localPort});
            }
            nci->SetValues("*", 0, TcpState::Closed, pid, processName);
        }
    }

    bool NetworkConnectionManager::CloseConnection(const NetworkConnectionInfo *connection)
    {
        // Only TCP connections can be closed
        if (connection->GetProtocol() != NetworkProtocol::TCP && connection->GetProtocol() != NetworkProtocol::TCPv6)
        {
            spdlog::warn("Cannot close UDP connection");
            return false;
        }

        if (connection->GetProtocol() == NetworkProtocol::TCP)
        {
            // IPv4
            MIB_TCPROW row{};
            row.dwState = MIB_TCP_STATE_DELETE_TCB;

            // Convert local address string to network byte order
            struct in_addr localAddr;
            if (inet_pton(AF_INET, connection->GetLocalAddress().c_str(), &localAddr) != 1)
            {
                spdlog::error("Failed to parse local address: {}", connection->GetLocalAddress());
                return false;
            }
            row.dwLocalAddr = localAddr.S_un.S_addr;
            row.dwLocalPort = htons(static_cast<u_short>(connection->GetLocalPort()));

            // Convert remote address string to network byte order
            struct in_addr remoteAddr;
            if (inet_pton(AF_INET, connection->GetRemoteAddress().c_str(), &remoteAddr) != 1)
            {
                spdlog::error("Failed to parse remote address: {}", connection->GetRemoteAddress());
                return false;
            }
            row.dwRemoteAddr = remoteAddr.S_un.S_addr;
            row.dwRemotePort = htons(static_cast<u_short>(connection->GetRemotePort()));

            DWORD result = SetTcpEntry(&row);
            if (result != NO_ERROR)
            {
                LogWin32ErrorCode("SetTcpEntry", result, "closing TCP connection {}:{} -> {}:{}",
                    connection->GetLocalAddress(), connection->GetLocalPort(),
                    connection->GetRemoteAddress(), connection->GetRemotePort());
                return false;
            }

            spdlog::info("Closed TCP connection {}:{} -> {}:{}",
                connection->GetLocalAddress(), connection->GetLocalPort(),
                connection->GetRemoteAddress(), connection->GetRemotePort());
            return true;
        }
        else
        {
            // IPv6 - SetTcpEntry doesn't support IPv6, would need SetTcp6Entry (undocumented)
            spdlog::warn("Closing TCPv6 connections is not supported by the Windows API");
            return false;
        }
    }

    std::string NetworkConnectionManager::GetProcessNameFromPid(DWORD pid)
    {
        if (pid == 0)
        {
            return "System Idle Process";
        }
        if (pid == 4)
        {
            return "System";
        }

        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess)
        {
            return std::format("PID {}", pid);
        }

        wchar_t processPath[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, processPath, &size))
        {
            // Extract just the filename
            wchar_t *fileName = wcsrchr(processPath, L'\\');
            if (fileName)
            {
                CloseHandle(hProcess);
                return utils::WideToUtf8(fileName + 1);
            }
            CloseHandle(hProcess);
            return utils::WideToUtf8(processPath);
        }

        CloseHandle(hProcess);
        return std::format("PID {}", pid);
    }

    std::string NetworkConnectionManager::FormatIPv4Address(DWORD addr)
    {
        if (addr == 0)
        {
            return "0.0.0.0";
        }

        // addr is in network byte order
        struct in_addr inAddr;
        inAddr.S_un.S_addr = addr;

        char buffer[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &inAddr, buffer, sizeof(buffer)))
        {
            return buffer;
        }

        return "?.?.?.?";
    }

    std::string NetworkConnectionManager::FormatIPv6Address(const BYTE addr[16])
    {
        // Check if all zeros
        bool allZero = true;
        for (int i = 0; i < 16; i++)
        {
            if (addr[i] != 0)
            {
                allZero = false;
                break;
            }
        }

        if (allZero)
        {
            return "::";
        }

        char buffer[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, addr, buffer, sizeof(buffer)))
        {
            return buffer;
        }

        return "?";
    }

} // namespace pserv
