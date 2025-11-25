/// @file async_operation.h
/// @brief Background task execution with progress reporting and cancellation.
///
/// AsyncOperation provides infrastructure for running long-running operations
/// in a background thread while keeping the UI responsive.
#pragma once

/// @brief Windows message sent when an async operation completes.
/// Posted to the HWND provided to AsyncOperation::Start().
#define WM_ASYNC_OPERATION_COMPLETE (WM_USER + 1)

namespace pserv
{
    /// @brief Execution state of an async operation.
    enum class AsyncStatus
    {
        Pending,   ///< Not yet started.
        Running,   ///< Currently executing.
        Completed, ///< Finished successfully.
        Cancelled, ///< Cancelled by user request.
        Failed     ///< Finished with an error.
    };

    /// @brief Background operation with progress tracking and cancellation support.
    ///
    /// AsyncOperation runs a work function in a background thread, providing:
    /// - Progress reporting (0-100%) with status messages
    /// - Cooperative cancellation via IsCancelRequested()
    /// - Window message notification on completion
    ///
    /// @par Usage:
    /// @code
    /// AsyncOperation op;
    /// op.Start(hWnd, [](AsyncOperation* self) {
    ///     for (int i = 0; i < 100; ++i) {
    ///         if (self->IsCancelRequested()) return false;
    ///         self->ReportProgress(i / 100.0f, "Processing...");
    ///         DoWork();
    ///     }
    ///     return true;
    /// });
    /// @endcode
    ///
    /// @note Non-copyable and non-movable. Create on heap if ownership transfer needed.
    class AsyncOperation
    {
    public:
        AsyncOperation() = default;
        ~AsyncOperation();

        // Non-copyable, non-movable
        AsyncOperation(const AsyncOperation &) = delete;
        AsyncOperation &operator=(const AsyncOperation &) = delete;
        AsyncOperation(AsyncOperation &&) = delete;
        AsyncOperation &operator=(AsyncOperation &&) = delete;

        /// @brief Start the operation in a background thread.
        /// @param hWnd Window to receive WM_ASYNC_OPERATION_COMPLETE on finish.
        /// @param workFunc Function to execute; receives this pointer for progress/cancel.
        /// @note workFunc should return true on success, false on failure.
        void Start(HWND hWnd, std::function<bool(AsyncOperation *)> workFunc);

        /// @brief Request cooperative cancellation.
        /// Sets a flag that workFunc should check via IsCancelRequested().
        void RequestCancel();

        /// @brief Check if cancellation was requested.
        /// @return true if RequestCancel() was called.
        /// @note Thread-safe; intended to be called from worker thread.
        bool IsCancelRequested() const;

        /// @brief Report progress from the worker thread.
        /// @param progress Completion ratio (0.0 to 1.0).
        /// @param message Status text to display in progress UI.
        void ReportProgress(float progress, std::string message);

        /// @brief Get current execution status.
        AsyncStatus GetStatus() const;

        /// @brief Get current progress value.
        /// @return Progress ratio from 0.0 to 1.0.
        float GetProgress() const;

        /// @brief Get the current progress status message.
        std::string GetProgressMessage() const;

        /// @brief Get error message if status is Failed.
        std::string GetErrorMessage() const;

        /// @brief Block until the operation completes.
        void Wait();

    private:
        std::atomic<AsyncStatus> m_status{AsyncStatus::Pending}; ///< Current execution state.
        std::atomic<bool> m_bCancelRequested{false};             ///< Cancellation flag.
        std::atomic<float> m_progress{0.0f};                     ///< Progress ratio.
        std::future<void> m_future;                              ///< Background task handle.
        HWND m_hWnd{nullptr};                                    ///< Window for completion message.

        mutable std::mutex m_mutex;     ///< Protects string members.
        std::string m_progressMessage;  ///< Current status text.
        std::string m_errorMessage;     ///< Error description if failed.
    };

} // namespace pserv
