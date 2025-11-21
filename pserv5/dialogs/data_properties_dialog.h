#pragma once
#include <models/service_info.h>

namespace pserv
{

    class DataController;

    class DataPropertiesDialog final
    {
    private:
        const std::vector<DataObject *> &m_dataObjects;
        DataController *m_controller{nullptr};
        int m_activeTabIndex{0};
        bool m_bOpen{false};

    public:
        DataPropertiesDialog(DataController *controller, const std::vector<DataObject *> &dataObjects);
        ~DataPropertiesDialog() = default;

        // Open the dialog for multiple services
        void Open();

        // Close the dialog
        void Close();

        // Render the dialog (call every frame)
        // Returns true if changes were applied
        bool Render();

    private:
        // Apply changes to a specific service
        bool ApplyChanges(DataObject *dataObject);

        // Initialize editable fields from service
        void InitializeFields(const DataObject *dataObject);

        // Render the content for a single service
        void RenderContent(const DataObject *dataObject);
    };

} // namespace pserv
