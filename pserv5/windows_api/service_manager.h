#pragma once
#include <string>
#include <vector>
#include <wil/resource.h>

namespace pserv {

class ServiceInfo;  // Forward declaration

class ServiceManager {
private:
    wil::unique_schandle m_hScManager;

public:
    ServiceManager();
    ~ServiceManager() = default;

    // Enumerate all services on the local machine
    // Returns raw pointers - caller is responsible for cleanup
    std::vector<ServiceInfo*> EnumerateServices();

    // Start a service and wait for it to reach running state
    // progressCallback is called periodically with progress (0.0-1.0) and status message
    // Returns true on success, throws exception on failure
    static bool StartServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback = nullptr);

    // Stop a service and wait for it to reach stopped state
    // progressCallback is called periodically with progress (0.0-1.0) and status message
    // Returns true on success, throws exception on failure
    static bool StopServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback = nullptr);

    // Pause a service and wait for it to reach paused state
    // progressCallback is called periodically with progress (0.0-1.0) and status message
    // Returns true on success, throws exception on failure
    static bool PauseServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback = nullptr);

    // Resume a service and wait for it to reach running state
    // progressCallback is called periodically with progress (0.0-1.0) and status message
    // Returns true on success, throws exception on failure
    static bool ResumeServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback = nullptr);

    // Restart a service (stop then start)
    // progressCallback is called periodically with progress (0.0-1.0) and status message
    // Returns true on success, throws exception on failure
    static bool RestartServiceByName(const std::string& serviceName, std::function<void(float, std::string)> progressCallback = nullptr);

    // Change service startup type
    // startType: SERVICE_AUTO_START, SERVICE_DEMAND_START, SERVICE_DISABLED, etc.
    // Returns true on success, throws exception on failure
    static bool ChangeServiceStartType(const std::string& serviceName, DWORD startType);

    // Delete a service
    // The service must be stopped before it can be deleted
    // Returns true on success, throws exception on failure
    static bool DeleteService(const std::string& serviceName);
};

} // namespace pserv
