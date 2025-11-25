/// @file data_properties_dialog.h
/// @brief Properties dialog for viewing and editing DataObject properties.
///
/// Provides a tabbed ImGui dialog for inspecting and modifying properties
/// of one or more selected DataObjects.
#pragma once

#ifndef PSERV_CONSOLE_BUILD
namespace pserv
{
    class DataController;

    /// @brief Record of a pending property edit.
    struct PropertyEdit
    {
        int tabIndex;         ///< Which object (tab) was edited.
        int columnIndex;      ///< Which column was edited.
        std::string newValue; ///< New value to apply.
    };

    /// @brief Modal dialog for viewing/editing DataObject properties.
    ///
    /// Features:
    /// - Tabbed interface for multi-selection
    /// - Read-only and editable fields based on column metadata
    /// - Text, multiline, integer, and combo box edit types
    /// - Action buttons for applicable operations
    /// - Transaction-based edit commit
    class DataPropertiesDialog final
    {
    private:
        const std::vector<DataObject *> &m_dataObjects;
        DataController *m_controller{nullptr};
        HWND m_hWnd{nullptr};
        int m_activeTabIndex{0};
        bool m_bOpen{false};

        std::vector<PropertyEdit> m_pendingEdits;
        std::map<int, std::map<int, std::string>> m_editBuffers; ///< [tabIndex][columnIndex] -> edit value.

    public:
        DataPropertiesDialog(DataController *controller, const std::vector<DataObject *> &dataObjects, HWND hWnd);
        ~DataPropertiesDialog() = default;

        /// @brief Open the dialog.
        void Open();

        /// @brief Close the dialog.
        void Close();

        /// @brief Check if dialog is currently open.
        bool IsOpen() const { return m_bOpen; }

        /// @brief Check if there are unsaved edits.
        bool HasPendingEdits() const { return !m_editBuffers.empty(); }

        /// @brief Render the dialog (call every frame).
        /// @return true if changes were applied.
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

#endif
