/// @file data_action_dispatch_context.h
/// @brief Context object passed to action execution methods.
///
/// DataActionDispatchContext carries all the state needed by a DataAction
/// to execute its operation, including selected objects and UI handles.
#pragma once

#ifdef PSERV_CONSOLE_BUILD
namespace argparse
{
    class ArgumentParser;
}
#endif

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
    /// - UI handles for displaying dialogs and progress
    /// - Flags for controlling post-action behavior
    ///
    /// @note Actions that perform long-running operations should set
    ///       m_bShowProgressDialog and use m_pAsyncOp for background work.
    class DataActionDispatchContext final
    {
    public:
        DataActionDispatchContext() = default;
        ~DataActionDispatchContext();

        HWND m_hWnd{nullptr};                        ///< Parent window handle for dialogs.
        AsyncOperation *m_pAsyncOp{nullptr};         ///< Async operation for background work.
        std::vector<DataObject *> m_selectedObjects; ///< Objects selected for this action.
        DataController *m_pController{nullptr};      ///< Controller that owns the objects.
        bool m_bShowProgressDialog{false};           ///< Show progress UI during execution.
        bool m_bNeedsRefresh{false};                 ///< Request data refresh after action.

#ifdef PSERV_CONSOLE_BUILD
        argparse::ArgumentParser *m_pActionParser{nullptr}; ///< CLI argument parser for this action.
#endif
    };
} // namespace pserv
