#pragma once
#include "../core/data_object.h"
#include <Windows.h>
#include <string>

namespace pserv {

enum class ServiceProperty {
    Name = 0,
    DisplayName,
    Status,
    StartType,
    ProcessId,
    ServiceType,
    BinaryPathName,
    Description
};

class ServiceInfo : public DataObject {
private:
    std::string m_name;
    std::string m_displayName;
    DWORD m_currentState;
    DWORD m_startType;
    DWORD m_processId;
    DWORD m_serviceType;
    std::string m_binaryPathName;
    std::string m_description;

public:
    ServiceInfo(std::string name, std::string displayName, DWORD currentState, DWORD serviceType);
    ~ServiceInfo() override = default;

    // DataObject interface
    std::string GetId() const override { return m_name; }
    void Update(const DataObject& other) override;
    std::string GetProperty(int propertyId) const override;

    // Getters
    const std::string& GetName() const { return m_name; }
    const std::string& GetDisplayName() const { return m_displayName; }
    DWORD GetCurrentState() const { return m_currentState; }
    DWORD GetStartType() const { return m_startType; }
    DWORD GetProcessId() const { return m_processId; }
    DWORD GetServiceType() const { return m_serviceType; }
    const std::string& GetBinaryPathName() const { return m_binaryPathName; }
    const std::string& GetDescription() const { return m_description; }

    // Setters
    void SetCurrentState(DWORD state);
    void SetStartType(DWORD startType) { m_startType = startType; }
    void SetProcessId(DWORD pid) { m_processId = pid; }
    void SetBinaryPathName(const std::string& path) { m_binaryPathName = path; }
    void SetDescription(const std::string& desc) { m_description = desc; }

    // Helpers
    std::string GetStatusString() const;
    std::string GetStartTypeString() const;
};

} // namespace pserv
