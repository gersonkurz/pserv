#pragma once
#include <models/service_info.h>

namespace pserv
{

    class ServicePropertiesDialog
    {
    private:
        struct ServiceEditorState
        {
            ServiceInfo *pService{nullptr};
            char displayName[256]{};
            char description[1024]{};
            char binaryPathName[1024]{};
            int startupType{0}; // 0=Automatic, 1=Manual, 2=Disabled
            bool bDirty{false};
        };

        bool m_bOpen{false};
        std::vector<ServiceEditorState> m_serviceStates;
        int m_activeTabIndex{0};

    public:
        ServicePropertiesDialog() = default;
        ~ServicePropertiesDialog() = default;

        // Open the dialog for multiple services
        void Open(const std::vector<ServiceInfo *> &services);

        // Close the dialog
        void Close();

        // Check if dialog is open
        bool IsOpen() const
        {
            return m_bOpen;
        }

        // Render the dialog (call every frame)
        // Returns true if changes were applied
        bool Render();

    private:
        // Apply changes to a specific service
        bool ApplyChanges(ServiceEditorState &state);

        // Initialize editable fields from service
        void InitializeFields(ServiceEditorState &state);

        // Render the content for a single service
        void RenderServiceContent(ServiceEditorState &state);

        // Map startup type enum to combo index
        int GetStartupTypeIndex(DWORD startType) const;

        // Map combo index to startup type enum
        DWORD GetStartupTypeFromIndex(int index) const;
    };

} // namespace pserv
