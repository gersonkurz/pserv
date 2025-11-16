#pragma once
#include <atomic>
#include <future>
#include <functional>
#include <string>
#include <mutex>
#include <Windows.h>

#define WM_ASYNC_OPERATION_COMPLETE (WM_USER + 1)

namespace pserv {

enum class AsyncStatus {
    Pending,
    Running,
    Completed,
    Cancelled,
    Failed
};

class AsyncOperation {
public:
    AsyncOperation() = default;
    ~AsyncOperation();

    // Non-copyable, non-movable
    AsyncOperation(const AsyncOperation&) = delete;
    AsyncOperation& operator=(const AsyncOperation&) = delete;
    AsyncOperation(AsyncOperation&&) = delete;
    AsyncOperation& operator=(AsyncOperation&&) = delete;

    // Start async operation
    // workFunc receives this AsyncOperation* to report progress and check cancellation
    // Returns true on success, false on failure
    void Start(HWND hWnd, std::function<bool(AsyncOperation*)> workFunc);

    // Request cancellation
    void RequestCancel();

    // Check if cancellation was requested (called from worker thread)
    bool IsCancelRequested() const;

    // Report progress from worker thread
    void ReportProgress(float progress, std::string message);

    // Get current status
    AsyncStatus GetStatus() const;

    // Get progress (0.0 to 1.0)
    float GetProgress() const;

    // Get progress message
    std::string GetProgressMessage() const;

    // Get error message (if failed)
    std::string GetErrorMessage() const;

    // Wait for completion (blocks)
    void Wait();

private:
    std::atomic<AsyncStatus> m_status{AsyncStatus::Pending};
    std::atomic<bool> m_bCancelRequested{false};
    std::atomic<float> m_progress{0.0f};
    std::future<void> m_future;
    HWND m_hWnd{nullptr};

    mutable std::mutex m_mutex;
    std::string m_progressMessage;
    std::string m_errorMessage;
};

} // namespace pserv
