/// @file service_manager.h
/// @brief Windows Service Control Manager API wrapper.
///
/// Provides service enumeration, control (start/stop/pause/resume),
/// and configuration management via the SCM API.
#pragma once

namespace pserv
{
    class DataObjectContainer;

    /// @brief Wrapper for Windows Service Control Manager operations.
    ///
    /// Provides both instance methods (for enumeration with a specific SCM handle)
    /// and static methods (for control operations that open their own handles).
    ///
    /// Supports local and remote machine connections.
    class ServiceManager
    {
    private:
        wil::unique_schandle m_hScManager;  ///< Handle to Service Control Manager.
        std::string m_machineName;           ///< Target machine (empty = local).

    public:
        /// @brief Open a connection to the Service Control Manager.
        /// @param machineName Target machine name (empty for local machine).
        ServiceManager(const std::string& machineName = "");
        ~ServiceManager() = default;

        /// @brief Get the connected machine name.
        const std::string& GetMachineName() const { return m_machineName; }

        /// @brief Check if SCM connection is valid.
        bool IsConnected() const { return m_hScManager != nullptr; }

        /// @brief Enumerate services into a container.
        /// @param doc Container to populate with ServiceInfo objects.
        /// @param serviceType SERVICE_WIN32, SERVICE_DRIVER, or combined flags.
        /// @param isAutoRefresh Suppress repeated error logging during auto-refresh.
        void EnumerateServices(DataObjectContainer *doc, DWORD serviceType = SERVICE_WIN32 | SERVICE_DRIVER, bool isAutoRefresh = false);

        /// @name Service Control Operations
        /// Static methods that open their own SCM handles.
        /// @param progressCallback Optional callback for progress updates (0.0-1.0).
        /// @throws std::runtime_error on failure.
        /// @{
        static bool StartServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback = nullptr);
        static bool StopServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback = nullptr);
        static bool PauseServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback = nullptr);
        static bool ResumeServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback = nullptr);
        static bool RestartServiceByName(const std::string &serviceName, std::function<void(float, std::string)> progressCallback = nullptr);
        /// @}

        /// @brief Change service startup type.
        /// @param startType SERVICE_AUTO_START, SERVICE_DEMAND_START, SERVICE_DISABLED, etc.
        static bool ChangeServiceStartType(const std::string &serviceName, DWORD startType);

        /// @brief Delete a service (must be stopped first).
        static bool DeleteService(const std::string &serviceName);

        /// @brief Change service configuration.
        static bool ChangeServiceConfig(
            const std::string &serviceName, const std::string &displayName, const std::string &description, DWORD startType, const std::string &binaryPathName);
    };

} // namespace pserv
