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
    Description,
    User,
    LoadOrderGroup,
    ErrorControl,
    TagId,
    Win32ExitCode,
    ServiceSpecificExitCode,
    CheckPoint,
    WaitHint,
    ServiceFlags,
    ControlsAccepted
};

class ServiceInfo : public DataObject {
private:
    std::string m_name;
    std::string m_displayName;
    DWORD m_currentState;
    DWORD m_startType;
    DWORD m_processId;
    DWORD m_serviceType;
    DWORD m_controlsAccepted;
    std::string m_binaryPathName;
    std::string m_description;

    // Additional service configuration data
    std::string m_user;              // ServiceStartName - account the service runs under
    std::string m_loadOrderGroup;    // Load order group
    DWORD m_errorControl;            // Error control level
    DWORD m_tagId;                   // Tag identifier
    DWORD m_win32ExitCode;           // Win32 exit code
    DWORD m_serviceSpecificExitCode; // Service-specific exit code
    DWORD m_checkPoint;              // Check point for lengthy operations
    DWORD m_waitHint;                // Estimated time for pending operation (ms)
    DWORD m_serviceFlags;            // Service flags

public:
    ServiceInfo(std::string name, std::string displayName, DWORD currentState, DWORD serviceType);
    ~ServiceInfo() override = default;

    // DataObject interface
    std::string GetId() const override { return m_name; }
    void Update(const DataObject& other) override;
    std::string GetProperty(int propertyId) const override;
    bool MatchesFilter(const std::string& filter) const override;

    // Getters
    const std::string& GetName() const { return m_name; }
    const std::string& GetDisplayName() const { return m_displayName; }
    DWORD GetCurrentState() const { return m_currentState; }
    DWORD GetStartType() const { return m_startType; }
    DWORD GetProcessId() const { return m_processId; }
    DWORD GetServiceType() const { return m_serviceType; }
    DWORD GetControlsAccepted() const { return m_controlsAccepted; }
    const std::string& GetBinaryPathName() const { return m_binaryPathName; }
    const std::string& GetDescription() const { return m_description; }
    const std::string& GetUser() const { return m_user; }
    const std::string& GetLoadOrderGroup() const { return m_loadOrderGroup; }
    DWORD GetErrorControl() const { return m_errorControl; }
    DWORD GetTagId() const { return m_tagId; }
    DWORD GetWin32ExitCode() const { return m_win32ExitCode; }
    DWORD GetServiceSpecificExitCode() const { return m_serviceSpecificExitCode; }
    DWORD GetCheckPoint() const { return m_checkPoint; }
    DWORD GetWaitHint() const { return m_waitHint; }
    DWORD GetServiceFlags() const { return m_serviceFlags; }

    // Setters
    void SetCurrentState(DWORD state);
    void SetDisplayName(const std::string& displayName) { m_displayName = displayName; }
    void SetStartType(DWORD startType) { m_startType = startType; }
    void SetProcessId(DWORD pid) { m_processId = pid; }
    void SetControlsAccepted(DWORD controls) { m_controlsAccepted = controls; }
    void SetBinaryPathName(const std::string& path) { m_binaryPathName = path; }
    void SetDescription(const std::string& desc) { m_description = desc; }
    void SetUser(const std::string& user) { m_user = user; }
    void SetLoadOrderGroup(const std::string& group) { m_loadOrderGroup = group; }
    void SetErrorControl(DWORD errorControl) { m_errorControl = errorControl; }
    void SetTagId(DWORD tagId) { m_tagId = tagId; }
    void SetWin32ExitCode(DWORD code) { m_win32ExitCode = code; }
    void SetServiceSpecificExitCode(DWORD code) { m_serviceSpecificExitCode = code; }
    void SetCheckPoint(DWORD checkPoint) { m_checkPoint = checkPoint; }
    void SetWaitHint(DWORD waitHint) { m_waitHint = waitHint; }
    void SetServiceFlags(DWORD flags) { m_serviceFlags = flags; }

    // Helpers
    std::string GetStatusString() const;
    std::string GetStartTypeString() const;
    std::string GetServiceTypeString() const;
    std::string GetErrorControlString() const;
    std::string GetControlsAcceptedString() const;
    std::string GetInstallLocation() const;  // Extract directory from binary path
};

} // namespace pserv
