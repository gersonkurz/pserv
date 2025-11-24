#include "precomp.h"
#include <actions/network_connection_actions.h>
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>
#include <models/network_connection_info.h>
#include <utils/string_utils.h>
#include <windows_api/network_connection_manager.h>

namespace pserv
{

    namespace
    {

        // Helper function to get NetworkConnectionInfo from DataObject
        inline const NetworkConnectionInfo *GetConnectionInfo(const DataObject *obj)
        {
            return static_cast<const NetworkConnectionInfo *>(obj);
        }

        // ============================================================================
        // Copy Actions
        // ============================================================================

        class NetworkConnectionCopyLocalAction final : public DataAction
        {
        public:
            NetworkConnectionCopyLocalAction()
                : DataAction{"Copy Local Endpoint", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                spdlog::error("'Copy Local Endpoint' is not supported in console build");
                throw std::runtime_error("This action requires clipboard and is not available in console mode");
#else
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *conn = GetConnectionInfo(ctx.m_selectedObjects[0]);
                std::string endpoint = conn->GetLocalEndpoint();
                utils::CopyToClipboard(endpoint.c_str());
                spdlog::info("Copied local endpoint to clipboard: {}", endpoint);
#endif
            }
        };

        class NetworkConnectionCopyRemoteAction final : public DataAction
        {
        public:
            NetworkConnectionCopyRemoteAction()
                : DataAction{"Copy Remote Endpoint", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *conn = GetConnectionInfo(obj);
                // Only available for TCP connections with remote endpoints
                return (conn->GetProtocol() == NetworkProtocol::TCP || conn->GetProtocol() == NetworkProtocol::TCPv6) &&
                       conn->GetRemoteAddress() != "*" && conn->GetRemoteAddress() != "0.0.0.0";
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                spdlog::error("'Copy Remote Endpoint' is not supported in console build");
                throw std::runtime_error("This action requires clipboard and is not available in console mode");
#else
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *conn = GetConnectionInfo(ctx.m_selectedObjects[0]);
                std::string endpoint = conn->GetRemoteEndpoint();
                utils::CopyToClipboard(endpoint.c_str());
                spdlog::info("Copied remote endpoint to clipboard: {}", endpoint);
#endif
            }
        };

        class NetworkConnectionCopyProcessAction final : public DataAction
        {
        public:
            NetworkConnectionCopyProcessAction()
                : DataAction{"Copy Process Name", ActionVisibility::ContextMenu}
            {
            }

            bool IsAvailableFor(const DataObject *) const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
#ifdef PSERV_CONSOLE_BUILD
                spdlog::error("'Copy Process Name' is not supported in console build");
                throw std::runtime_error("This action requires clipboard and is not available in console mode");
#else
                if (ctx.m_selectedObjects.empty())
                    return;

                const auto *conn = GetConnectionInfo(ctx.m_selectedObjects[0]);
                utils::CopyToClipboard(conn->GetProcessName().c_str());
                spdlog::info("Copied process name to clipboard: {}", conn->GetProcessName());
#endif
            }
        };

        // ============================================================================
        // Close Connection Action (placeholder)
        // ============================================================================

        class NetworkConnectionCloseAction final : public DataAction
        {
        public:
            NetworkConnectionCloseAction()
                : DataAction{"Close Connection", ActionVisibility::Both}
            {
            }

            bool IsAvailableFor(const DataObject *obj) const override
            {
                const auto *conn = GetConnectionInfo(obj);
                // Only TCP connections can be closed
                return (conn->GetProtocol() == NetworkProtocol::TCP || conn->GetProtocol() == NetworkProtocol::TCPv6) &&
                       conn->GetState() != TcpState::Listen &&
                       conn->GetState() != TcpState::Closed;
            }

            bool IsDestructive() const override
            {
                return true;
            }

            void Execute(DataActionDispatchContext &ctx) const override
            {
                // TODO: Implement TCP connection closing
                // Requires parsing addresses back to binary format and calling SetTcpEntry
                spdlog::error("Closing TCP connections is not yet implemented");
#ifndef PSERV_CONSOLE_BUILD
                MessageBoxA(ctx.m_hWnd,
                    "Closing TCP connections is not yet implemented.\n\n"
                    "This requires converting address strings back to binary format\n"
                    "and calling SetTcpEntry with MIB_TCP_STATE_DELETE_TCB state.\n\n"
                    "This will be implemented in a future update.",
                    "Not Yet Implemented",
                    MB_OK | MB_ICONINFORMATION);
#endif
                throw std::runtime_error("Closing TCP connections is not yet implemented");
            }
        };

        // Static action instances
        NetworkConnectionCopyLocalAction theCopyLocalAction;
        NetworkConnectionCopyRemoteAction theCopyRemoteAction;
        NetworkConnectionCopyProcessAction theCopyProcessAction;
        NetworkConnectionCloseAction theCloseAction;

    } // anonymous namespace

    // ============================================================================
    // Factory Function
    // ============================================================================

    std::vector<const DataAction *> CreateNetworkConnectionActions(NetworkProtocol protocol, TcpState state)
    {
        std::vector<const DataAction *> actions;

        // Copy actions
        actions.push_back(&theCopyLocalAction);

        // Remote endpoint only for TCP with valid remote address
        if (protocol == NetworkProtocol::TCP || protocol == NetworkProtocol::TCPv6)
        {
            actions.push_back(&theCopyRemoteAction);
        }

        actions.push_back(&theCopyProcessAction);

        actions.push_back(&theDataActionSeparator);

        // Close action (only for active TCP connections)
        if ((protocol == NetworkProtocol::TCP || protocol == NetworkProtocol::TCPv6) &&
            state != TcpState::Listen && state != TcpState::Closed)
        {
            actions.push_back(&theCloseAction);
        }

        return actions;
    }

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> CreateAllNetworkConnectionActions()
    {
        // Console: Return all actions regardless of connection state
        // Note: All actions are currently GUI-only (clipboard) or unimplemented (close)
        return {
            &theCopyLocalAction,
            &theCopyRemoteAction,
            &theCopyProcessAction,
            &theDataActionSeparator,
            &theCloseAction};
    }
#endif

} // namespace pserv
