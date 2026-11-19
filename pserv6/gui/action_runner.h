#pragma once
// Runs DataActions for the pserv6 window (plan Phase 5, D4): everything the user is asked happens
// here before Execute() (confirmation, parameters), the persistent dispatch context is kept while
// an asynchronous operation runs (with a progress dialog and Cancel), and the frontend services
// actions call back into (IActionHost: messages, the properties dialog) are provided.
//
// One dialog at a time: ContentDialogs are shown one after another (WinUI allows a single open
// dialog per window). A new action is refused while one is being prepared or is running.
#include <deque>
#include <core/action_host.h>
#include <core/data_action.h>
#include <core/data_action_dispatch_context.h>

namespace pserv6
{
    /// Holds one reference to each object of a selection for as long as it lives.
    class RetainedSelection final
    {
    public:
        RetainedSelection() = default;
        explicit RetainedSelection(const std::vector<pserv::DataObject *> &objects);
        ~RetainedSelection();
        RetainedSelection(RetainedSelection &&other) noexcept : m_objects{std::exchange(other.m_objects, {})} {}
        RetainedSelection &operator=(RetainedSelection &&other) noexcept;
        RetainedSelection(const RetainedSelection &) = delete;
        RetainedSelection &operator=(const RetainedSelection &) = delete;
        const std::vector<pserv::DataObject *> &Get() const { return m_objects; }

    private:
        std::vector<pserv::DataObject *> m_objects;
    };

    class ActionRunner final : public pserv::IActionHost
    {
    public:
        struct Callbacks
        {
            /// Root element of the window, for the dialogs' XamlRoot.
            std::function<winrt::Microsoft::UI::Xaml::XamlRoot()> xamlRoot;
            /// The window handle (owner of shell launches and the file dialog; receives
            /// WM_ASYNC_OPERATION_COMPLETE, forwarded to OnAsyncComplete()).
            HWND hwnd{nullptr};
            /// Pause/resume background refreshes (while a dialog or an action is open).
            std::function<void(bool)> setRefreshPaused;
            /// Refresh a controller after an action or an edit changed the system.
            std::function<void(pserv::DataController *)> refresh;
            /// Status line text.
            std::function<void(const std::string &)> status;
            /// True if the controller's objects live on another machine. Their mutating actions
            /// would run against this machine (TODO 19), so only read-only ones are allowed.
            std::function<bool(const pserv::DataController *)> isRemote;
            /// Bumped whenever the target machine changes: a request, menu or properties dialog
            /// prepared before the change must not run afterwards on what the rows now are.
            std::function<uint64_t()> targetGeneration;
        };

        /// Actions allowed on a remote machine's objects: Properties (read-only) and the copy and
        /// export actions, which only read the objects.
        static bool IsReadOnlyAction(const pserv::DataAction *pAction);

        explicit ActionRunner(Callbacks callbacks);
        ~ActionRunner() override;
        ActionRunner(const ActionRunner &) = delete;
        ActionRunner &operator=(const ActionRunner &) = delete;

        /// Confirm, collect parameters, then execute. Ignored while another action is pending.
        /// @param generation The target generation the selection was taken in (default: now);
        ///        the request is refused if the target changed since.
        void RequestAction(const pserv::DataAction *pAction, pserv::DataController *pController, const std::vector<pserv::DataObject *> &selection,
            std::optional<uint64_t> generation = {});

        /// True while an action is being prepared, runs, or a dialog is open.
        bool IsBusy() const { return m_bBusy || m_bDialogOpen; }

        /// Take the single dialog slot for a dialog of the window's own (e.g. remote connect),
        /// and keep it through follow-up work; false if busy. ReleaseDialog() gives it back.
        bool ReserveDialog();
        void ReleaseDialog();

        /// The window received WM_ASYNC_OPERATION_COMPLETE.
        void OnAsyncComplete();

        /// Stop showing anything (the window closes); a running operation is waited for.
        void Shutdown();

        // IActionHost
        void ShowMessage(pserv::ActionMessageKind kind, std::string_view title, std::string_view text) override;
        void ShowProperties(pserv::DataActionDispatchContext &ctx) override;

        /// Automation (the action self-test): messages are recorded instead of shown, and no progress
        /// dialog appears. TakeMessages() returns and clears the recorded messages.
        void SetAutomation(bool bAutomation) { m_bAutomation = bAutomation; }
        std::vector<std::string> TakeMessages() { return std::exchange(m_recorded, {}); }

        /// Self-test / automation: execute without dialogs (confirmation implied, parameters given).
        void ExecuteNow(const pserv::DataAction *pAction, pserv::DataController *pController, const std::vector<pserv::DataObject *> &selection,
            std::map<std::string, std::string> parameters);

    private:
        winrt::fire_and_forget RunRequest(const pserv::DataAction *pAction, pserv::DataController *pController, RetainedSelection selection, uint64_t generation);
        uint64_t CurrentGeneration() const { return m_callbacks.targetGeneration ? m_callbacks.targetGeneration() : 0; }
        bool RefuseIfTargetChanged(uint64_t generation, const std::string &what);
        winrt::fire_and_forget ShowNextMessage();
        winrt::fire_and_forget RunProperties(pserv::DataController *pController, RetainedSelection selection);
        void Execute(const pserv::DataAction *pAction, pserv::DataController *pController, const std::vector<pserv::DataObject *> &selection,
            std::map<std::string, std::string> parameters);
        void ShowProgress();
        void FinishAsync();
        winrt::Microsoft::UI::Xaml::Controls::ContentDialog NewDialog(const std::string &title);
        void SetDialogOpen(bool bOpen);

        Callbacks m_callbacks;
        pserv::DataActionDispatchContext m_dispatch; ///< Persistent: an async operation lives in it.
        bool m_bBusy{false};                         ///< An action is being prepared or runs.
        bool m_bDialogOpen{false};
        std::deque<std::pair<std::string, std::string>> m_messages; ///< Waiting for the dialog slot.
        std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)}; ///< Cleared by Shutdown().
        winrt::Microsoft::UI::Xaml::Controls::ContentDialog m_progressDialog{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ProgressBar m_progressBar{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBlock m_progressText{nullptr};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_progressTimer{nullptr};
        bool m_bAsyncFinished{false}; ///< FinishAsync() ran for the current operation.
        std::string m_actionName;     ///< The action being executed (dialog titles, messages).
        bool m_bAutomation{false};
        std::vector<std::string> m_recorded; ///< Messages in automation mode.
    };
} // namespace pserv6
