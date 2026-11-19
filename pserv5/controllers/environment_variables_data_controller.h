/// @file environment_variables_data_controller.h
/// @brief Controller for system and user environment variables.
///
/// Manages environment variables from the Windows registry with support
/// for viewing, editing, adding, and deleting variables.
#pragma once
#include <core/data_controller.h>
#include <models/environment_variable_info.h>

namespace pserv
{
    /// @brief Data controller for environment variables.
    ///
    /// Reads environment variables from two registry locations:
    /// - HKCU\\Environment (user variables)
    /// - HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment (system variables)
    ///
    /// Supports full CRUD operations:
    /// - View all user and system environment variables
    /// - Edit variable values in the properties dialog
    /// - Add new variables via dedicated dialog
    /// - Delete existing variables
    ///
    /// System variables are highlighted; user variables show in normal color.
    class EnvironmentVariablesDataController : public DataController
    {
    private:
        /// @brief Buffer for staged property edits.
        struct EditBuffer
        {
            std::string name;
            std::string value;
        };
        EditBuffer m_editBuffer;
        DataObject *m_editingObject = nullptr; ///< Object being edited.


    public:
        EnvironmentVariablesDataController();
        ~EnvironmentVariablesDataController() override = default;

        void Refresh(bool isAutoRefresh = false) override;
        std::vector<const DataAction *> GetActions(const DataObject *dataObject) const override;

        std::vector<const DataAction *> GetAllActions() const override;

        VisualState GetVisualState(const DataObject *dataObject) const override;

        void BeginPropertyEdits(DataObject *obj) override;
        bool SetPropertyEdit(DataObject *obj, int columnIndex, const std::string &newValue) override;
        bool CommitPropertyEdits(DataObject *obj) override;

    };

} // namespace pserv
