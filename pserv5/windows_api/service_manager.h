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
};

} // namespace pserv
