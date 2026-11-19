#include "precomp.h"
#include <core/refresh_coordinator.h>

namespace pserv
{
    // ---------------------------------------------------------------- RefreshWorker

    RefreshWorker::RefreshWorker()
        : m_thread{[this] { Run(); }}
    {
    }

    RefreshWorker::~RefreshWorker()
    {
        {
            std::lock_guard lock{m_mutex};
            m_bStop = true;
            m_queue.clear();
        }
        m_wake.notify_one();
        m_thread.join();
    }

    void RefreshWorker::Post(std::function<void()> task)
    {
        {
            std::lock_guard lock{m_mutex};
            m_queue.push_back(std::move(task));
        }
        m_wake.notify_one();
    }

    void RefreshWorker::Run()
    {
        for (;;)
        {
            std::function<void()> task;
            {
                std::unique_lock lock{m_mutex};
                m_wake.wait(lock, [this] { return m_bStop || !m_queue.empty(); });
                if (m_bStop)
                    return;
                task = std::move(m_queue.front());
                m_queue.pop_front();
            }
            try
            {
                task();
            }
            catch (const std::exception &e)
            {
                spdlog::error("Refresh worker task failed: {}", e.what());
            }
        }
    }

    // ---------------------------------------------------------------- RefreshCoordinator

    RefreshCoordinator::RefreshCoordinator(std::function<void(Task)> runOnWorker, std::function<void(Task)> postToUi)
        : m_runOnWorker{std::move(runOnWorker)},
          m_postToUi{std::move(postToUi)}
    {
    }

    bool RefreshCoordinator::IsInFlight(const DataController *pController) const
    {
        return m_inFlight.contains(pController);
    }

    void RefreshCoordinator::Request(DataController *pController, bool bAutoRefresh)
    {
        if (!pController)
            return;
        if (m_bPaused || m_inFlight.contains(pController))
        {
            // Coalesced: one refresh after the current one completes, or after unpausing.
            // A manual request wins over an automatic one.
            auto [it, inserted] = m_pending.try_emplace(pController, bAutoRefresh);
            if (!inserted)
                it->second = it->second && bAutoRefresh;
            return;
        }

        const RefreshRequest request = pController->CreateRefreshRequest(bAutoRefresh);
        m_inFlight.insert(pController);
        m_runOnWorker([this, pController, request]
            {
                pController->RefreshShadow(request);
                m_postToUi([this, pController, request] { Complete(pController, request); });
            });
    }

    void RefreshCoordinator::Complete(DataController *pController, const RefreshRequest &request)
    {
        m_inFlight.erase(pController);

        if (m_bPaused)
        {
            // A dialog or action is open: never change what the user is looking at. Refresh again
            // once unpaused.
            m_pending.try_emplace(pController, request.bAutoRefresh);
            return;
        }

        const PublishResult result = pController->Publish(request);
        if (result == PublishResult::Published && m_onPublished)
        {
            m_onPublished(pController);
        }
        else if (result == PublishResult::Failed && m_onFailed)
        {
            m_onFailed(pController);
        }

        // A stale result (mutation, target change, view switch) is refreshed again at once, as is a
        // request that arrived while this one was in flight. A failure is not retried at once (that
        // would loop as fast as the enumeration fails); the next request tries again.
        const auto pending = m_pending.find(pController);
        if (result == PublishResult::Stale || pending != m_pending.end())
        {
            const bool bAuto = pending != m_pending.end() ? pending->second : request.bAutoRefresh;
            if (pending != m_pending.end())
                m_pending.erase(pending);
            Request(pController, bAuto);
        }
    }

    void RefreshCoordinator::SetPaused(bool bPaused)
    {
        m_bPaused = bPaused;
        if (m_bPaused)
            return;
        auto pending = std::exchange(m_pending, {});
        for (const auto &[pController, bAuto] : pending)
        {
            Request(pController, bAuto);
        }
    }
} // namespace pserv
