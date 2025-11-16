#pragma once
#include "../models/service_info.h"
#include <string>
#include <memory>

namespace pserv {

class ServicePropertiesDialog {
private:
    bool m_bOpen{false};
    ServiceInfo* m_pService{nullptr};

    // Editable fields
    char m_displayName[256]{};
    char m_description[1024]{};
    char m_binaryPathName[1024]{};
    int m_startupType{0};  // 0=Automatic, 1=Manual, 2=Disabled

    // Track if any changes were made
    bool m_bDirty{false};

public:
    ServicePropertiesDialog() = default;
    ~ServicePropertiesDialog() = default;

    // Open the dialog for a specific service
    void Open(ServiceInfo* service);

    // Close the dialog
    void Close();

    // Check if dialog is open
    bool IsOpen() const { return m_bOpen; }

    // Render the dialog (call every frame)
    // Returns true if changes were applied
    bool Render();

private:
    // Apply changes to the service
    bool ApplyChanges();

    // Initialize editable fields from service
    void InitializeFields();

    // Map startup type enum to combo index
    int GetStartupTypeIndex(DWORD startType) const;

    // Map combo index to startup type enum
    DWORD GetStartupTypeFromIndex(int index) const;
};

} // namespace pserv
