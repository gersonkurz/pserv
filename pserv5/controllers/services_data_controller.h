#pragma once
#include <core/data_controller.h>

namespace pserv
{

    class ServicesDataController : public DataController
    {
    private:
        const DWORD m_serviceType{SERVICE_WIN32 | SERVICE_DRIVER}; // Default: all services

        // Edit buffer for property transactions
        struct EditBuffer
        {
            std::string displayName;
            std::string description;
            std::string startType;
            std::string binaryPathName;
            DWORD startTypeValue{SERVICE_NO_CHANGE};
        };
        EditBuffer m_editBuffer;
        DataObject *m_editingObject{nullptr};

    public:
        ServicesDataController(DWORD serviceType = SERVICE_WIN32, const char *viewName = nullptr, const char *itemName = nullptr);

    private:
        // DataController interface
        void Refresh() override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

        // Get visual state for a service (for coloring)
        VisualState GetVisualState(const DataObject *service) const override;

        // Property editing transaction methods
        void BeginPropertyEdits(DataObject *obj) override;
        bool SetPropertyEdit(DataObject *obj, int columnIndex, const std::string &newValue) override;
        bool CommitPropertyEdits(DataObject *obj) override;
        std::vector<std::string> GetComboOptions(int columnIndex) const override;
    };

} // namespace pserv
