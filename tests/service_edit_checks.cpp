// Regression checks use a fake SCM boundary. They never modify Windows services.
#include "precomp.h"
#include <controllers/services_data_controller.h>
#include <models/service_info.h>
#include <windows_api/service_manager.h>
#include <core/data_action.h>
#include <iostream>

namespace
{
    bool configSucceeds = false;
    std::string submittedDisplayName;
    std::string submittedMachine;
    void Require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }
}

namespace pserv
{
    ServiceManager::ServiceManager(const std::string& machineName) : m_machineName(machineName) {}
    void ServiceManager::EnumerateServices(DataObjectContainer*, DWORD, bool)
    {
        throw std::runtime_error("Enumeration is not expected in editing regression checks");
    }
    bool ServiceManager::ChangeServiceConfig(const std::string&, const std::string& displayName,
        const std::string&, DWORD, const std::string&, const std::string& machineName)
    {
        submittedDisplayName = displayName;
        submittedMachine = machineName;
        return configSucceeds;
    }
    std::vector<const DataAction*> CreateServiceActions(DWORD, DWORD) { return {}; }
}

int main()
{
    try
    {
        std::unique_ptr<pserv::DataController> controller = std::make_unique<pserv::ServicesDataController>();
        pserv::ServiceInfo service("TestService");
        service.SetValues("Original display name", SERVICE_STOPPED, SERVICE_WIN32_OWN_PROCESS);
        service.SetStartType(SERVICE_DEMAND_START);
        controller->BeginPropertyEdits(&service);
        Require(!controller->SetPropertyEdit(&service, 0, "RenamedIdentity"), "Service identity must not be editable");
        Require(controller->SetPropertyEdit(&service, 1, "New display name"), "Display-name column must be editable");
        Require(!controller->SetPropertyEdit(&service, 3, "Invalid"), "Invalid startup type must be rejected");
        Require(!controller->CommitPropertyEdits(&service), "SCM failure must be returned to the frontend");
        Require(submittedDisplayName == "New display name", "The intended display name must reach SCM");
        Require(service.GetDisplayName() == "Original display name", "Failed commit must not change the displayed model");
        Require(service.GetName() == "TestService", "Service identity must remain stable");

        Require(submittedMachine.empty(), "Local edits must target the local SCM");
        static_cast<pserv::ServicesDataController*>(controller.get())->SetMachineName("test-host");
        configSucceeds = true;
        controller->BeginPropertyEdits(&service);
        Require(controller->SetPropertyEdit(&service, 1, "Committed display name"), "Valid display-name edit rejected");
        Require(controller->SetPropertyEdit(&service, 3, "Automatic"), "Valid startup edit rejected");
        Require(controller->CommitPropertyEdits(&service), "Successful SCM operation must commit");
        Require(service.GetDisplayName() == "Committed display name", "Successful commit must update the model");
        Require(submittedMachine == "test-host", "Remote edits must target the selected machine");
        Require(service.GetStartType() == SERVICE_AUTO_START, "Startup type must update after success");
        std::cout << "Service editing regression checks passed; no system services were modified.\n";
        return 0;
    }
    catch (std::exception const& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
