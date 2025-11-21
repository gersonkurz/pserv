#include "precomp.h"
#include <core/async_operation.h>

namespace pserv
{

    AsyncOperation::~AsyncOperation()
    {
        if (m_future.valid())
        {
            RequestCancel();
            m_future.wait();
        }
    }

    void AsyncOperation::Start(HWND hWnd, std::function<bool(AsyncOperation *)> workFunc)
    {
        if (m_status != AsyncStatus::Pending)
        {
            spdlog::warn("AsyncOperation already started");
            return;
        }

        m_hWnd = hWnd;
        m_status = AsyncStatus::Running;
        m_progress = 0.0f;

        // Launch async task
        m_future = std::async(std::launch::async,
            [this, workFunc]()
            {
                try
                {
                    bool success = workFunc(this);

                    if (m_bCancelRequested)
                    {
                        m_status = AsyncStatus::Cancelled;
                    }
                    else if (success)
                    {
                        m_status = AsyncStatus::Completed;
                        m_progress = 1.0f;
                    }
                    else
                    {
                        m_status = AsyncStatus::Failed;
                    }
                }
                catch (const std::exception &e)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_errorMessage = e.what();
                    m_status = AsyncStatus::Failed;
                    spdlog::error("AsyncOperation failed with exception: {}", e.what());
                }
                catch (...)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_errorMessage = "Unknown error";
                    m_status = AsyncStatus::Failed;
                    spdlog::error("AsyncOperation failed with unknown exception");
                }

                // Notify completion on UI thread
                if (m_hWnd)
                {
                    PostMessage(m_hWnd, WM_ASYNC_OPERATION_COMPLETE, 0, 0);
                }
            });
    }

    void AsyncOperation::RequestCancel()
    {
        m_bCancelRequested = true;
    }

    bool AsyncOperation::IsCancelRequested() const
    {
        return m_bCancelRequested;
    }

    void AsyncOperation::ReportProgress(float progress, std::string message)
    {
        m_progress = progress;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_progressMessage = std::move(message);
        }
        spdlog::debug("AsyncOperation progress: {:.1f}% - {}", progress * 100, m_progressMessage);
    }

    AsyncStatus AsyncOperation::GetStatus() const
    {
        return m_status;
    }

    float AsyncOperation::GetProgress() const
    {
        return m_progress;
    }

    std::string AsyncOperation::GetProgressMessage() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_progressMessage;
    }

    std::string AsyncOperation::GetErrorMessage() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_errorMessage;
    }

    void AsyncOperation::Wait()
    {
        if (m_future.valid())
        {
            m_future.wait();
        }
    }

} // namespace pserv
