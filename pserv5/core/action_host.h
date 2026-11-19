/// @file action_host.h
/// @brief Services a frontend provides to actions while they execute.
#pragma once

namespace pserv
{
    class DataActionDispatchContext;

    /// @brief Severity of a message an action reports.
    enum class ActionMessageKind
    {
        Information,
        Warning,
        Error
    };

    /// @brief Implemented by each frontend (console, ImGui, WinUI) and passed to actions through
    /// DataActionDispatchContext::m_pHost.
    ///
    /// Only non-interactive services live here: confirmation and input are collected before
    /// Execute() (see data_action.h). Implementations must not block waiting for the user.
    ///
    /// Error contract for actions: a **failure** is thrown as std::exception from Execute() (or
    /// reported through AsyncOperation) and the frontend reports it; ShowMessage() is for
    /// non-fatal notes only (e.g. "IPv6 connection skipped"), so nothing is reported twice.
    class IActionHost
    {
    public:
        virtual ~IActionHost() = default;

        /// @brief Tell the user about an outcome (an error, or e.g. "IPv6 not supported").
        virtual void ShowMessage(ActionMessageKind kind, std::string_view title, std::string_view text) = 0;

        /// @brief Open the frontend's properties UI for ctx's selection. Frontends without one
        /// never offer theDataPropertiesAction, so they may ignore this.
        virtual void ShowProperties(DataActionDispatchContext &ctx) = 0;
    };
} // namespace pserv
