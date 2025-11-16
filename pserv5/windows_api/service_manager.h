#pragma once
#include <string>
#include <vector>
#include <wil/resource.h>

namespace pserv {

class ServiceManager {
private:
    wil::unique_schandle m_hScManager;

public:
    ServiceManager();
    ~ServiceManager() = default;

    // Enumerate all services on the local machine
    struct ServiceInfo {
        std::string name;
        std::string displayName;
        DWORD currentState;
        DWORD serviceType;
    };

    std::vector<ServiceInfo> EnumerateServices();
};

} // namespace pserv
