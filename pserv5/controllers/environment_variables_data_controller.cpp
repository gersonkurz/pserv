#include "precomp.h"
#include <actions/environment_variable_actions.h>
#include <controllers/environment_variables_data_controller.h>
#include <models/environment_variable_info.h>
#include <windows_api/environment_variable_manager.h>

namespace pserv
{

    EnvironmentVariablesDataController::EnvironmentVariablesDataController()
        : DataController{"Environment Variables",
              "Environment Variable",
              {{"Name", "Name", ColumnDataType::String, true, ColumnEditType::Text},
                  {"Value", "Value", ColumnDataType::String, true, ColumnEditType::Text},
                  {"Scope", "Scope", ColumnDataType::String}}}
    {
    }

    void EnvironmentVariablesDataController::Refresh()
    {
        spdlog::info("Refreshing environment variables...");

        Clear();

        try
        {
            m_objects = EnvironmentVariableManager::EnumerateEnvironmentVariables();

            // Re-apply last sort order if any
            if (m_lastSortColumn >= 0)
            {
                Sort(m_lastSortColumn, m_lastSortAscending);
            }

            spdlog::info("Successfully refreshed {} environment variables", m_objects.size());
            m_bLoaded = true;
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to refresh environment variables: {}", e.what());
        }
    }

    std::vector<const DataAction *> EnvironmentVariablesDataController::GetActions(const DataObject *dataObject) const
    {
        const auto *envVar = static_cast<const EnvironmentVariableInfo *>(dataObject);
        return CreateEnvironmentVariableActions(envVar->GetScope());
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

            // If name changed, we need to delete old and create new
            if (m_editBuffer.name != envVar->GetName())
            {
                // Delete old variable
                if (!EnvironmentVariableManager::DeleteEnvironmentVariable(envVar->GetName(), envVar->GetScope()))
                {
                    spdlog::error("Failed to delete old environment variable");
                    m_editingObject = nullptr;
                    return false;
                }
            }

            // Set the variable (either update existing or create renamed one)
            if (!EnvironmentVariableManager::SetEnvironmentVariable(m_editBuffer.name, m_editBuffer.value, envVar->GetScope()))
            {
                spdlog::error("Failed to set environment variable");
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
