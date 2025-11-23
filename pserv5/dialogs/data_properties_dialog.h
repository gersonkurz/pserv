#pragma once

namespace pserv
{

    class DataController;

    struct PropertyEdit
    {
        int tabIndex;    // Which object (tab) was edited
        int columnIndex; // Which column was edited
        std::string newValue;
    };

    class DataPropertiesDialog final
    {
    private:
        const std::vector<DataObject *> &m_dataObjects;
        DataController *m_controller{nullptr};
        HWND m_hWnd{nullptr};
        int m_activeTabIndex{0};
        bool m_bOpen{false};

        // Edit tracking
        std::vector<PropertyEdit> m_pendingEdits;
        std::map<int, std::map<int, std::string>> m_editBuffers; // [tabIndex][columnIndex] -> current edit value

    public:
        DataPropertiesDialog(DataController *controller, const std::vector<DataObject *> &dataObjects, HWND hWnd);
        ~DataPropertiesDialog() = default;

        // Open the dialog for multiple services
        void Open();

        // Close the dialog
        void Close();

        // Check if dialog is open
        bool IsOpen() const
        {
            return m_bOpen;
        }

        // Check if there are unsaved edits
        bool HasPendingEdits() const
        {
            return !m_editBuffers.empty();
        }

        // Render the dialog (call every frame)
        // Returns true if changes were applied
        bool Render();

    private:
        // Apply all pending edits using transaction pattern
        bool ApplyAllEdits();

        // Check if a specific field has pending edits
        bool HasPendingEdit(int tabIndex, int columnIndex) const;

        // Get edit buffer value for a field (or original value if not edited)
        std::string GetEditValue(int tabIndex, int columnIndex) const;

        // Record an edit to a field
        void RecordEdit(int tabIndex, int columnIndex, const std::string &newValue);

        // Render the content for a single service
        void RenderContent(const DataObject *dataObject);

        // Render action buttons for the current object
        void RenderActionButtons(const DataObject *dataObject);
    };

} // namespace pserv
