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
              {{"Name", "Name", ColumnDataType::String, true, ColumnEditType::Text},
                  {"Value", "Value", ColumnDataType::String, true, ColumnEditType::Text},
                  {"Scope", "Scope", ColumnDataType::String}}}
    {
    }

    void EnvironmentVariablesDataController::Refresh(bool isAutoRefresh)
    {
        spdlog::info("Refreshing environment variables...");

        try
        {
            // Note: We don't call Clear() here - StartRefresh/FinishRefresh handles
            // update-in-place for existing objects and removes stale ones
            m_objects.StartRefresh();
            EnvironmentVariableManager::EnumerateEnvironmentVariables(&m_objects);
            m_objects.FinishRefresh();

            // Re-apply last sort order if any
            if (m_lastSortColumn >= 0)
            {
                Sort(m_lastSortColumn, m_lastSortAscending);
            }

            spdlog::info("Successfully refreshed {} environment variables", m_objects.GetSize());
            SetLoaded();
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

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> EnvironmentVariablesDataController::GetAllActions() const
    {
        return CreateAllEnvironmentVariableActions();
    }
#endif

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

#ifndef PSERV_CONSOLE_BUILD
    void EnvironmentVariablesDataController::ShowAddVariableDialog(EnvironmentVariableScope scope)
    {
        m_bShowAddDialog = true;
        m_addDialogScope = scope;
        m_addNameBuffer[0] = '\0';
        m_addValueBuffer[0] = '\0';
        m_addDialogError.clear();
    }

    void EnvironmentVariablesDataController::RenderAddVariableDialog(HWND hWnd)
    {
        if (m_bShowAddDialog)
        {
            ImGui::OpenPopup("Add Environment Variable");
            m_bShowAddDialog = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(450, 0));

        if (ImGui::BeginPopupModal("Add Environment Variable", nullptr, ImGuiWindowFlags_NoResize))
        {
            // Escape key to close dialog
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                m_addDialogError.clear();
                ImGui::CloseCurrentPopup();
            }

            const char *scopeStr = (m_addDialogScope == EnvironmentVariableScope::System) ? "System" : "User";
            ImGui::Text("Add new %s environment variable:", scopeStr);
            ImGui::Spacing();

            ImGui::Text("Name:");
            ImGui::SetNextItemWidth(-1);
            bool nameEnterPressed = ImGui::InputText("##VarName", m_addNameBuffer, sizeof(m_addNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();
            ImGui::Text("Value:");
            ImGui::SetNextItemWidth(-1);
            bool valueEnterPressed = ImGui::InputText("##VarValue", m_addValueBuffer, sizeof(m_addValueBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

            // Show error message if any
            if (!m_addDialogError.empty())
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::TextWrapped("%s", m_addDialogError.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool shouldAdd = (nameEnterPressed || valueEnterPressed) && strlen(m_addNameBuffer) > 0;

            if (ImGui::Button("Add", ImVec2(120, 0)) || shouldAdd)
            {
                if (strlen(m_addNameBuffer) == 0)
                {
                    m_addDialogError = "Variable name cannot be empty.";
                }
                else
                {
                    // Check if variable already exists
                    std::string name = m_addNameBuffer;
                    std::string lowerName = name;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    bool exists = false;
                    for (const auto *obj : m_objects)
                    {
                        const auto *envVar = static_cast<const EnvironmentVariableInfo *>(obj);
                        std::string existingName = envVar->GetName();
                        std::transform(existingName.begin(), existingName.end(), existingName.begin(), ::tolower);
                        if (envVar->GetScope() == m_addDialogScope && existingName == lowerName)
                        {
                            exists = true;
                            break;
                        }
                    }

                    if (exists)
                    {
                        m_addDialogError = std::format("Variable '{}' already exists in {} scope.", name, scopeStr);
                    }
                    else
                    {
                        // Try to create the variable
                        if (EnvironmentVariableManager::SetEnvironmentVariable(m_addNameBuffer, m_addValueBuffer, m_addDialogScope))
                        {
                            spdlog::info("Created new {} environment variable: {}", scopeStr, m_addNameBuffer);
                            m_addDialogError.clear();
                            Refresh(false);
                            ImGui::CloseCurrentPopup();
                        }
                        else
                        {
                            m_addDialogError = std::format("Failed to create variable. You may need administrator privileges for {} variables.", scopeStr);
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                m_addDialogError.clear();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
#endif

} // namespace pserv
