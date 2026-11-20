#include "precomp.h"
#include <actions/environment_variable_actions.h>
#include <controllers/environment_variables_data_controller.h>
#include <models/environment_variable_info.h>
#include <windows_api/environment_variable_manager.h>

namespace pserv
{

    EnvironmentVariablesDataController::EnvironmentVariablesDataController()
        : DataController{ENVIRONMENT_VARIABLES_CONTROLLER_NAME,
              "Environment Variable",
              {{EnvironmentVariableProperty::Name, "Name", "Name", ColumnDataType::String, true, ColumnEditType::Text},
                  {EnvironmentVariableProperty::Value, "Value", "Value", ColumnDataType::String, true, ColumnEditType::Text},
                  {EnvironmentVariableProperty::Scope, "Scope", "Scope", ColumnDataType::String}}}
    {
    }

    void EnvironmentVariablesDataController::Collect(DataObjectContainer &target, const RefreshRequest &request)
    {
        target.StartRefresh();
        EnvironmentVariableManager::EnumerateEnvironmentVariables(&target);
        target.FinishRefresh();
    }

    std::vector<const DataAction *> EnvironmentVariablesDataController::GetActions(const DataObject *dataObject) const
    {
        const auto *envVar = static_cast<const EnvironmentVariableInfo *>(dataObject);
        return CreateEnvironmentVariableActions(envVar->GetScope());
    }

    std::vector<const DataAction *> EnvironmentVariablesDataController::GetAllActions() const
    {
        return CreateAllEnvironmentVariableActions();
    }

    VisualState EnvironmentVariablesDataController::GetVisualState(const DataObject *dataObject) const
    {
        if (!dataObject)
        {
            return VisualState::Normal;
        }

        const auto *envVar = static_cast<const EnvironmentVariableInfo *>(dataObject);

        // Highlight system variables
        if (envVar->GetScope() == EnvironmentVariableScope::System)
        {
            return VisualState::Highlighted;
        }

        return VisualState::Normal;
    }

    void EnvironmentVariablesDataController::BeginPropertyEdits(DataObject *obj)
    {
        m_editingObject = obj;
        m_editBuffer = EditBuffer{}; // Clear buffer

        // Initialize buffer with current values
        auto *envVar = static_cast<EnvironmentVariableInfo *>(obj);
        m_editBuffer.name = envVar->GetName();
        m_editBuffer.value = envVar->GetValue();

        spdlog::info("BeginPropertyEdits for environment variable: {}", envVar->GetName());
    }

    bool EnvironmentVariablesDataController::SetPropertyEdit(DataObject *obj, int columnIndex, const std::string &newValue)
    {
        if (obj != m_editingObject)
        {
            spdlog::error("SetPropertyEdit called with wrong object");
            return false;
        }

        // Column indices based on constructor order:
        // 0=Name, 1=Value, 2=Scope (read-only)

        switch (columnIndex)
        {
        case 0: // Name
            m_editBuffer.name = newValue;
            spdlog::debug("Set Name = {}", newValue);
            return true;

        case 1: // Value
            m_editBuffer.value = newValue;
            spdlog::debug("Set Value = {}", newValue);
            return true;

        default:
            spdlog::warn("Attempted to edit non-editable column: {}", columnIndex);
            return false;
        }
    }

    bool EnvironmentVariablesDataController::CommitPropertyEdits(DataObject *obj)
    {
        if (obj != m_editingObject)
        {
            spdlog::error("CommitPropertyEdits called with wrong object");
            return false;
        }

        auto *envVar = static_cast<EnvironmentVariableInfo *>(obj);

        try
        {
            spdlog::info("Committing property edits for environment variable: {}", envVar->GetName());

            const std::string oldName = envVar->GetName();
            // Variable names are case-insensitive: a change of case only is an update in place
            const bool bRenamed = _stricmp(m_editBuffer.name.c_str(), oldName.c_str()) != 0;
            if (m_editBuffer.name.empty())
            {
                spdlog::error("An environment variable needs a name");
                m_editingObject = nullptr;
                return false;
            }
            if (bRenamed)
            {
                // Never overwrite another variable of the same scope
                for (const auto *other : m_objects)
                {
                    const auto *otherVar = static_cast<const EnvironmentVariableInfo *>(other);
                    if (otherVar != envVar && otherVar->GetScope() == envVar->GetScope() && _stricmp(otherVar->GetName().c_str(), m_editBuffer.name.c_str()) == 0)
                    {
                        spdlog::error("Cannot rename '{}': a variable '{}' already exists", oldName, m_editBuffer.name);
                        m_editingObject = nullptr;
                        return false;
                    }
                }
            }

            // Write the new (or updated) variable first: if that fails, nothing has changed
            if (!EnvironmentVariableManager::SetEnvironmentVariable(m_editBuffer.name, m_editBuffer.value, envVar->GetScope()))
            {
                spdlog::error("Failed to set environment variable");
                m_editingObject = nullptr;
                return false;
            }

            // Then remove the old name; if that fails, undo the new one so the rename is all or nothing
            if (bRenamed && !EnvironmentVariableManager::DeleteEnvironmentVariable(oldName, envVar->GetScope()))
            {
                spdlog::error("Failed to delete the old environment variable '{}'; the rename is undone", oldName);
                EnvironmentVariableManager::DeleteEnvironmentVariable(m_editBuffer.name, envVar->GetScope());
                m_editingObject = nullptr;
                return false;
            }

            // Update local object with new values
            envVar->SetName(m_editBuffer.name);
            envVar->SetValue(m_editBuffer.value);

            spdlog::info("Successfully committed property edits for environment variable: {}", envVar->GetName());
            m_editingObject = nullptr;
            return true;
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to commit property edits: {}", e.what());
            m_editingObject = nullptr;
            return false;
        }
    }


} // namespace pserv
