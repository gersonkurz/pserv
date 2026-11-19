/// @file com_scope.h
/// @brief RAII COM initialization for a scope.
#pragma once

namespace pserv::utils
{
    /// @brief Initializes COM on the current thread for this object's lifetime.
    ///
    /// Declare it **before** any COM pointer in the same scope. Locals are destroyed in reverse
    /// order, so every pointer is released before CoUninitialize() runs. Releasing an in-process
    /// COM object after the thread's last CoUninitialize() can call into an unloaded DLL and crash
    /// with an access violation, which manual CoUninitialize() calls on early-return paths did.
    class ComScope final
    {
    public:
        explicit ComScope(DWORD coinit)
            : m_hr{CoInitializeEx(nullptr, coinit)}
        {
        }

        ~ComScope()
        {
            if (SUCCEEDED(m_hr))
                CoUninitialize();
        }

        ComScope(const ComScope &) = delete;
        ComScope &operator=(const ComScope &) = delete;

        /// @brief COM is usable on this thread: initialized here, already initialized (S_FALSE),
        /// or already initialized with a different apartment model (RPC_E_CHANGED_MODE).
        bool IsUsable() const { return SUCCEEDED(m_hr) || m_hr == RPC_E_CHANGED_MODE; }

        /// @brief The CoInitializeEx result, for logging.
        HRESULT Result() const { return m_hr; }

    private:
        const HRESULT m_hr;
    };
} // namespace pserv::utils
