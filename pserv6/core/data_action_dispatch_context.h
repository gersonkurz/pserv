/// @file data_action_dispatch_context.h
/// @brief Context object passed to action execution methods.
///
/// DataActionDispatchContext carries all the state needed by a DataAction
/// to execute its operation, including selected objects and frontend services.
#pragma once
#include <core/action_host.h>

namespace pserv
{
    class DataController;
    class DataObject;
    class AsyncOperation;

    /// @brief Execution context passed to DataAction::Execute().
    ///
    /// This context provides actions with everything they need to execute:
    /// - The selected DataObjects to operate on
    /// - The owning DataController for accessing data and metadata
    /// - The frontend's services (m_pHost) and the parameters collected before dispatch
    /// - Flags for controlling post-action behavior
    ///
    /// @note **Ownership:** the context owns one reference to each object in m_selectedObjects
    ///       and releases them in its destructor. Whoever fills m_selectedObjects must Retain()
    ///       each object it adds.
    /// @note Actions that perform long-running operations should set
    ///       m_bShowProgressDialog and use m_pAsyncOp for background work.
    class DataActionDispatchContext final
    {
    public:
        DataActionDispatchContext() = default;
        ~DataActionDispatchContext();

        DataActionDispatchContext(const DataActionDispatchContext &) = delete;
        DataActionDispatchContext &operator=(const DataActionDispatchContext &) = delete;

        HWND m_hWnd{nullptr};                        ///< Owner window for shell launches, and AsyncOperation completion target; nullptr in the console.
        IActionHost *m_pHost{nullptr};               ///< Frontend services; may be nullptr (messages are then only logged).
        AsyncOperation *m_pAsyncOp{nullptr};         ///< Async operation for background work.
        std::vector<DataObject *> m_selectedObjects; ///< Objects selected for this action (owned references, see above).
        DataController *m_pController{nullptr};      ///< Controller that owns the objects.
        std::map<std::string, std::string> m_parameters; ///< Values for the action's GetParameters(), by name.
        bool m_bShowProgressDialog{false};           ///< Show progress UI during execution.
        bool m_bNeedsRefresh{false};                 ///< Request data refresh after action.

        /// @brief Value collected for parameter `name`, or empty if none was provided.
        std::string GetParameter(const std::string &name) const;

        /// @brief Report a message through the host; always logged as well.
        void ShowMessage(ActionMessageKind kind, std::string_view title, std::string_view text);

        /// @brief Replace the selection, retaining the new objects and releasing the old ones.
        void SetSelection(const std::vector<DataObject *> &objects);
    };
} // namespace pserv
