#include "precomp.h"
#include <core/data_action.h>
#include <core/data_controller.h>
#include <core/data_object.h>
#include <core/data_action_dispatch_context.h>
#include <dialogs/data_properties_dialog.h>

namespace pserv
{

    DataPropertiesDialog::DataPropertiesDialog(DataController *controller, const std::vector<DataObject *> &dataObjects, HWND hWnd)
        : m_controller{controller},
          m_dataObjects{dataObjects},
          m_hWnd{hWnd}
    {
    }

    void DataPropertiesDialog::Open()
    {
        if (m_dataObjects.empty())
        {
            spdlog::warn("DataPropertiesDialog::Open() called with empty objects");
            return;
        }

        m_activeTabIndex = 0;
        m_bOpen = true;
        m_pendingEdits.clear();
        m_editBuffers.clear();

        spdlog::info("DataPropertiesDialog::Open() - opened dialog with {} objects", m_dataObjects.size());
    }

    void DataPropertiesDialog::Close()
    {
        spdlog::info("DataPropertiesDialog::Close() called - m_bOpen was {}", m_bOpen);
        m_bOpen = false;
        m_activeTabIndex = 0;
        m_pendingEdits.clear();
        m_editBuffers.clear();
    }

    bool DataPropertiesDialog::Render()
    {
        if (m_dataObjects.empty())
        {
            spdlog::warn("DataPropertiesDialog::Render() - no objects to render");
            return false;
        }

        // Only render if dialog is still open
        if (!m_bOpen)
        {
            spdlog::info("DataPropertiesDialog::Render() - dialog closed, not rendering");
            return false;
        }

        bool changesApplied = false;

        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        std::string windowTitle = m_dataObjects.size() == 1 ? std::format("{} Properties", m_controller->GetItemName())
                                                            : std::format("{} Properties ({} services)", m_controller->GetItemName(), m_dataObjects.size());

        if (ImGui::Begin(windowTitle.c_str(), &m_bOpen, ImGuiWindowFlags_NoCollapse))
        {
            // Escape key to close dialog
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                m_bOpen = false;
            }

            // Enter key to apply changes (same as OK button)
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            {
                spdlog::info("DataPropertiesDialog: Enter key pressed, applying changes");
                changesApplied = ApplyAllEdits();
                Close();
            }

            // Tab bar for multiple services
            if (m_dataObjects.size() > 1)
            {
                // Use scrolling tabs if there are many services
                ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_FittingPolicyScroll;
                if (ImGui::BeginTabBar("ServiceTabs", tabBarFlags))
                {
                    for (size_t i = 0; i < m_dataObjects.size(); ++i)
                    {
                        const auto dataObject = m_dataObjects[i];
                        std::string tabLabel = dataObject->GetItemName();
                        /*if (state.bDirty) {
                            tabLabel += " *";  // Mark modified tabs
                        }*/

                        if (ImGui::BeginTabItem(tabLabel.c_str()))
                        {
                            m_activeTabIndex = static_cast<int>(i);
                            RenderContent(dataObject);
                            ImGui::EndTabItem();
                        }
                    }
                    ImGui::EndTabBar();
                }
            }
            else
            {
                // Single service - no tabs needed
                RenderContent(m_dataObjects[0]);
            }

            ImGui::Separator();

            // === BUTTONS ===
            ImGui::Spacing();

            // Count dirty tabs
            int dirtyCount = 0;
            /*
            for (const auto& state : m_serviceStates) {
                if (state.bDirty) dirtyCount++;
            }
            // Apply button
            std::string applyLabel = dirtyCount > 0
                ? std::format("Apply ({} modified)", dirtyCount)
                : "Apply";

            if (ImGui::Button(applyLabel.c_str(), ImVec2(150, 0))) {
                for (auto& state : m_dataObjects) {
                    if (state.bDirty) {
                        if (ApplyChanges(state)) {
                            changesApplied = true;
                            state.bDirty = false;
                        }
                    }
                }
            }
            */

            ImGui::SameLine();
            if (ImGui::Button("OK", ImVec2(100, 0)))
            {
                spdlog::info("DataPropertiesDialog: OK button clicked");
                changesApplied = ApplyAllEdits();
                Close();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
            {
                spdlog::info("DataPropertiesDialog: Cancel button clicked");
                Close();
            }
        }
        ImGui::End();

        // Check if window was closed by X button
        if (!m_bOpen)
        {
            spdlog::info("DataPropertiesDialog: X button clicked (m_bOpen set to false by ImGui)");
            Close();
        }

        return changesApplied;
    }

    void DataPropertiesDialog::RenderContent(const DataObject *dataObject)
    {
        if (!dataObject || !m_controller)
            return;

        // Get all columns from controller
        const auto &columns = m_controller->GetColumns();

        // Render all properties in a single collapsing header
        if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const float labelWidth = 200.0f;

            // Loop through each column and render appropriate control
            for (size_t i = 0; i < columns.size(); ++i)
            {
                const auto &column = columns[i];
                int columnIndex = static_cast<int>(i);

                // Display label
                ImGui::Text("%s:", column.DisplayName.c_str());
                ImGui::SameLine(labelWidth);
                ImGui::SetNextItemWidth(-1);

                // Create a unique ID for each field
                std::string fieldId = std::format("##{}", i);

                // Check if this column is editable
                if (column.Editable)
                {
                    // Get current value (from edit buffer or original)
                    std::string value = GetEditValue(m_activeTabIndex, columnIndex);

                    // Add visual indication for editable fields
                    // Make editable fields noticeably lighter/brighter than read-only
                    ImVec4 baseColor = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

                    // Calculate brightness to determine if dark or light theme
                    float brightness = baseColor.x * 0.299f + baseColor.y * 0.587f + baseColor.z * 0.114f;
                    bool isDarkTheme = brightness < 0.5f;

                    // Adjust brightness: lighter in dark mode, darker in light mode
                    float adjustment = isDarkTheme ? 1.7f : 0.8f;

                    ImVec4 editableBg = ImVec4(
                        std::min(baseColor.x * adjustment, 1.0f),
                        std::min(baseColor.y * adjustment, 1.0f),
                        std::min(baseColor.z * adjustment, 1.0f),
                        baseColor.w
                    );

                    ImVec4 editableHovered = ImVec4(
                        std::min(editableBg.x * 1.15f, 1.0f),
                        std::min(editableBg.y * 1.15f, 1.0f),
                        std::min(editableBg.z * 1.15f, 1.0f),
                        baseColor.w
                    );

                    ImVec4 editableActive = ImVec4(
                        std::min(editableBg.x * 1.3f, 1.0f),
                        std::min(editableBg.y * 1.3f, 1.0f),
                        std::min(editableBg.z * 1.3f, 1.0f),
                        baseColor.w
                    );

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, editableBg);
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, editableHovered);
                    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, editableActive);

                    // Render based on edit type
                    switch (column.EditType)
                    {
                    case ColumnEditType::Text:
                    {
                        // Single-line text input (editable)
                        char buffer[1024];
                        strncpy_s(buffer, value.c_str(), sizeof(buffer) - 1);
                        buffer[sizeof(buffer) - 1] = '\0';

                        if (ImGui::InputText(fieldId.c_str(), buffer, sizeof(buffer)))
                        {
                            RecordEdit(m_activeTabIndex, columnIndex, buffer);
                        }
                        break;
                    }

                    case ColumnEditType::TextMultiline:
                    {
                        // Multi-line text input (editable)
                        char buffer[4096];
                        strncpy_s(buffer, value.c_str(), sizeof(buffer) - 1);
                        buffer[sizeof(buffer) - 1] = '\0';

                        if (ImGui::InputTextMultiline(fieldId.c_str(), buffer, sizeof(buffer), ImVec2(-1, 100)))
                        {
                            RecordEdit(m_activeTabIndex, columnIndex, buffer);
                        }
                        break;
                    }

                    case ColumnEditType::Integer:
                    {
                        // Integer input (editable)
                        int intValue = 0;
                        try
                        {
                            intValue = std::stoi(value);
                        }
                        catch (...)
                        {
                        }

                        if (ImGui::InputInt(fieldId.c_str(), &intValue))
                        {
                            RecordEdit(m_activeTabIndex, columnIndex, std::to_string(intValue));
                        }
                        break;
                    }

                    case ColumnEditType::Combo:
                    {
                        // Combo box (editable)
                        auto options = m_controller->GetComboOptions(columnIndex);
                        if (!options.empty())
                        {
                            // Find current selection index
                            int currentIndex = 0;
                            for (size_t j = 0; j < options.size(); ++j)
                            {
                                if (options[j] == value)
                                {
                                    currentIndex = static_cast<int>(j);
                                    break;
                                }
                            }

                            // Render combo box
                            if (ImGui::BeginCombo(fieldId.c_str(), value.c_str()))
                            {
                                for (size_t j = 0; j < options.size(); ++j)
                                {
                                    bool isSelected = (currentIndex == static_cast<int>(j));
                                    if (ImGui::Selectable(options[j].c_str(), isSelected))
                                    {
                                        RecordEdit(m_activeTabIndex, columnIndex, options[j]);
                                    }
                                    if (isSelected)
                                    {
                                        ImGui::SetItemDefaultFocus();
                                    }
                                }
                                ImGui::EndCombo();
                            }
                        }
                        else
                        {
                            // No options available, show as read-only
                            ImGui::InputText(fieldId.c_str(), const_cast<char *>(value.c_str()), value.size() + 1, ImGuiInputTextFlags_ReadOnly);
                        }
                        break;
                    }

                    default:
                        // Fallback to read-only
                        ImGui::InputText(fieldId.c_str(), const_cast<char *>(value.c_str()), value.size() + 1, ImGuiInputTextFlags_ReadOnly);
                        break;
                    }

                    // Pop the editable field colors
                    ImGui::PopStyleColor(3);
                }
                else
                {
                    // Read-only field
                    std::string value = dataObject->GetProperty(columnIndex);

                    if (value.empty())
                    {
                        ImGui::TextDisabled("N/A");
                    }
                    else
                    {
                        // Check if value contains newlines
                        bool hasNewlines = value.find('\n') != std::string::npos;

                        if (hasNewlines)
                        {
                            // Use InputTextMultiline for multiline content
                            int lineCount = static_cast<int>(std::count(value.begin(), value.end(), '\n')) + 1;
                            float height = ImGui::GetTextLineHeightWithSpacing() * std::min(lineCount, 5);

                            ImGui::InputTextMultiline(fieldId.c_str(),
                                                      const_cast<char *>(value.c_str()),
                                                      value.size() + 1,
                                                      ImVec2(-1, height),
                                                      ImGuiInputTextFlags_ReadOnly);
                        }
                        else
                        {
                            // Use single-line InputText for simple values
                            ImGui::InputText(fieldId.c_str(),
                                             const_cast<char *>(value.c_str()),
                                             value.size() + 1,
                                             ImGuiInputTextFlags_ReadOnly);
                        }
                    }
                }

                // Add separator between fields
                if (i < columns.size() - 1)
                {
                    ImGui::Separator();
                }
            }
        }

        // Render action buttons
        RenderActionButtons(dataObject);
    }

    void DataPropertiesDialog::RenderActionButtons(const DataObject *dataObject)
    {
        if (!m_controller)
            return;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Get all actions from controller for this specific object
        const auto actions = m_controller->GetActions(dataObject);

        // Collect actions that should appear in properties dialog
        std::vector<const DataAction *> dialogActions;
        for (const auto &action : actions)
        {
            auto visibility = action->GetVisibility();
            bool visibleInDialog = (static_cast<int>(visibility) & static_cast<int>(ActionVisibility::PropertiesDialog)) != 0;

            if (!visibleInDialog)
                continue;

            if (!action->IsAvailableFor(dataObject))
                continue;

            dialogActions.push_back(action);
        }

        // Render as individual buttons if there are any actions
        if (!dialogActions.empty())
        {
            bool firstButton = true;
            for (const auto &action : dialogActions)
            {
                if (action->IsSeparator())
                {
                    // Add spacing between button groups
                    ImGui::SameLine();
                    ImGui::Spacing();
                    ImGui::SameLine();
                    continue;
                }

                // Place buttons on same line (except first one)
                if (!firstButton)
                {
                    ImGui::SameLine();
                }
                firstButton = false;

                // Color destructive actions red
                if (action->IsDestructive())
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                }

                if (ImGui::Button(action->GetName().c_str()))
                {
                    // Build dispatch context for this action
                    DataActionDispatchContext ctx;
                    ctx.m_pController = m_controller;
                    ctx.m_selectedObjects = {const_cast<DataObject *>(dataObject)};
                    ctx.m_hWnd = m_hWnd;
                    ctx.m_pAsyncOp = nullptr;
                    ctx.m_bShowProgressDialog = false;

                    // Execute action
                    action->Execute(ctx);
                }

                if (action->IsDestructive())
                {
                    ImGui::PopStyleColor(3);
                }
            }
        }
    }

    bool DataPropertiesDialog::HasPendingEdit(int tabIndex, int columnIndex) const
    {
        auto tabIt = m_editBuffers.find(tabIndex);
        if (tabIt == m_editBuffers.end())
            return false;

        return tabIt->second.find(columnIndex) != tabIt->second.end();
    }

    std::string DataPropertiesDialog::GetEditValue(int tabIndex, int columnIndex) const
    {
        // Check if there's an edit in the buffer
        auto tabIt = m_editBuffers.find(tabIndex);
        if (tabIt != m_editBuffers.end())
        {
            auto colIt = tabIt->second.find(columnIndex);
            if (colIt != tabIt->second.end())
            {
                return colIt->second;
            }
        }

        // No edit, return original value
        return m_dataObjects[tabIndex]->GetProperty(columnIndex);
    }

    void DataPropertiesDialog::RecordEdit(int tabIndex, int columnIndex, const std::string &newValue)
    {
        m_editBuffers[tabIndex][columnIndex] = newValue;
        spdlog::debug("Recorded edit: tab={}, column={}, value={}", tabIndex, columnIndex, newValue);
    }

    bool DataPropertiesDialog::ApplyAllEdits()
    {
        if (m_editBuffers.empty())
        {
            spdlog::info("No edits to apply");
            return false;
        }

        bool anyChangesApplied = false;

        // Process each edited object
        for (const auto &[tabIndex, columnEdits] : m_editBuffers)
        {
            if (columnEdits.empty())
                continue;

            DataObject *obj = m_dataObjects[tabIndex];
            spdlog::info("Applying {} edits to object at tab {}", columnEdits.size(), tabIndex);

            // Begin transaction
            m_controller->BeginPropertyEdits(obj);

            // Set all edits for this object
            bool allSucceeded = true;
            for (const auto &[columnIndex, newValue] : columnEdits)
            {
                if (!m_controller->SetPropertyEdit(obj, columnIndex, newValue))
                {
                    spdlog::warn("Failed to set property edit: column={}", columnIndex);
                    allSucceeded = false;
                }
            }

            // Commit transaction
            if (allSucceeded)
            {
                if (m_controller->CommitPropertyEdits(obj))
                {
                    spdlog::info("Successfully committed edits for tab {}", tabIndex);
                    anyChangesApplied = true;
                }
                else
                {
                    spdlog::error("Failed to commit edits for tab {}", tabIndex);
                    MessageBoxA(nullptr, "Failed to apply property changes", "Error", MB_OK | MB_ICONERROR);
                }
            }
        }

        return anyChangesApplied;
    }

} // namespace pserv
