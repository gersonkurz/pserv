/// @file refresh_coordinator.h
/// @brief Background refresh protocol (plan D5, section 5.1), frontend-neutral.
#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <core/data_controller.h>

namespace pserv
{
    /// @brief A single background thread that runs posted tasks in order.
    ///
    /// The managers keep process-wide caches (e.g. the module cache) that are not thread-safe, so
    /// all refreshes must run on **one** worker: never run RefreshShadow() concurrently.
    class RefreshWorker final
    {
    public:
        RefreshWorker();
        ~RefreshWorker(); ///< Finishes the running task, drops queued ones, joins the thread.

        RefreshWorker(const RefreshWorker &) = delete;
        RefreshWorker &operator=(const RefreshWorker &) = delete;

        void Post(std::function<void()> task);

    private:
        void Run();

        std::mutex m_mutex;
        std::condition_variable m_wake;
        std::deque<std::function<void()>> m_queue;
        bool m_bStop{false};
        std::thread m_thread;
    };

    /// @brief Drives DataController's request / RefreshShadow / Publish protocol.
    ///
    /// All member functions must be called on the UI thread. The two functions passed in decide
    /// where work runs: `runOnWorker` must run tasks one at a time off the UI thread (e.g. a
    /// RefreshWorker); `postToUi` must run tasks later on the UI thread (e.g. a DispatcherQueue).
    /// Tests pass queues they drain by hand, which makes every interleaving deterministic.
    ///
    /// Rules (plan section 5.1):
    /// - one refresh in flight per controller; a request while one is in flight is coalesced into
    ///   a single follow-up refresh;
    /// - a result is published only if the controller's epoch is unchanged (see
    ///   DataController::InvalidatePendingRefreshes); a stale result is discarded and refreshed again;
    /// - while paused (a dialog or action is open), results are discarded and refreshes deferred;
    ///   SetPaused(false) requests one refresh for each controller that needed one.
    ///
    /// Lifetime: the coordinator, its controllers and the two executors must outlive every task
    /// they have accepted.
    class RefreshCoordinator final
    {
    public:
        using Task = std::function<void()>;

        RefreshCoordinator(std::function<void(Task)> runOnWorker, std::function<void(Task)> postToUi);

        RefreshCoordinator(const RefreshCoordinator &) = delete;
        RefreshCoordinator &operator=(const RefreshCoordinator &) = delete;

        void Request(DataController *pController, bool bAutoRefresh = false);
        void SetPaused(bool bPaused);
        bool IsPaused() const { return m_bPaused; }
        bool IsInFlight(const DataController *pController) const;

        /// @brief Called on the UI thread after each successful publish (only then).
        std::function<void(DataController *)> m_onPublished;

        /// @brief Called on the UI thread when a refresh failed (enumeration or merge). Nothing is
        /// retried automatically: the next manual or automatic request tries again.
        std::function<void(DataController *)> m_onFailed;

    private:
        void Complete(DataController *pController, const RefreshRequest &request);

        std::function<void(Task)> m_runOnWorker;
        std::function<void(Task)> m_postToUi;
        std::set<const DataController *> m_inFlight;
        std::map<DataController *, bool> m_pending; ///< Needs a refresh after the current one / after unpausing (value: auto-refresh).
        bool m_bPaused{false};
    };
} // namespace pserv
