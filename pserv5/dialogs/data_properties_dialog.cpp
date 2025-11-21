#include "precomp.h"
#include <core/data_action.h>
#include <core/data_controller.h>
#include <core/data_object.h>
#include <core/data_action_dispatch_context.h>
#include <dialogs/data_properties_dialog.h>

namespace pserv
{

    DataPropertiesDialog::DataPropertiesDialog(DataController *controller, const std::vector<DataObject *> &dataObjects)
        : m_controller{controller},
          m_dataObjects{dataObjects}
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

        spdlog::info("DataPropertiesDialog::Open() - opened dialog with {} objects", m_dataObjects.size());
    }

    void DataPropertiesDialog::Close()
    {
        spdlog::info("DataPropertiesDialog::Close() called - m_bOpen was {}", m_bOpen);
        m_bOpen = false;
        m_activeTabIndex = 0;
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
                // Apply all dirty changes
                for (auto dataObject : m_dataObjects)
                {
                    // if (state.bDirty) {
                    if (ApplyChanges(dataObject))
                    {
                        changesApplied = true;
                    }
                    //}
                }
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

            // Loop through each column and display as read-only field
            for (size_t i = 0; i < columns.size(); ++i)
            {
                const auto &column = columns[i];
                std::string value = dataObject->GetProperty(static_cast<int>(i));

                // Display label
                ImGui::Text("%s:", column.DisplayName.c_str());
                ImGui::SameLine(labelWidth);

                // Display value in a selectable/copyable input field (read-only)
                ImGui::SetNextItemWidth(-1);
                if (value.empty())
                {
                    ImGui::TextDisabled("N/A");
                }
                else
                {
                    // Create a unique ID for each field
                    std::string fieldId = std::format("##{}", i);

                    // Check if value contains newlines
                    bool hasNewlines = value.find('\n') != std::string::npos;

                    if (hasNewlines)
                    {
                        // Use InputTextMultiline for multiline content
                        int lineCount = std::count(value.begin(), value.end(), '\n') + 1;
                        float height = ImGui::GetTextLineHeightWithSpacing() * std::min(lineCount, 5);

                        ImGui::InputTextMultiline(fieldId.c_str(),
                                                const_cast<char*>(value.c_str()),
                                                value.size() + 1,
                                                ImVec2(-1, height),
                                                ImGuiInputTextFlags_ReadOnly);
                    }
                    else
                    {
                        // Use single-line InputText for simple values
                        ImGui::InputText(fieldId.c_str(),
                                       const_cast<char*>(value.c_str()),
                                       value.size() + 1,
                                       ImGuiInputTextFlags_ReadOnly);
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

        // Filter and render actions visible in properties dialog
        for (const auto &action : actions)
        {
            auto visibility = action->GetVisibility();
            bool visibleInDialog = (static_cast<int>(visibility) & static_cast<int>(ActionVisibility::PropertiesDialog)) != 0;

            if (!visibleInDialog)
                continue;

            if (!action->IsAvailableFor(dataObject))
                continue;

            if (action->IsSeparator())
            {
                ImGui::Spacing();
                continue;
            }

            // Color destructive actions red
            if (action->IsDestructive())
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
            }

            if (ImGui::Button(action->GetName().c_str()))
            {
                // Build dispatch context for this action
                DataActionDispatchContext ctx;
                ctx.m_pController = m_controller;
                ctx.m_selectedObjects = {const_cast<DataObject *>(dataObject)};
                ctx.m_hWnd = nullptr; // TODO: Get window handle if needed
                ctx.m_pAsyncOp = nullptr;
                ctx.m_bShowProgressDialog = false;

                // Execute action
                action->Execute(ctx);

                // Note: We don't close the dialog here - let the action decide
                // Some actions might want to close dialog, others might not
            }

            if (action->IsDestructive())
            {
                ImGui::PopStyleColor(3);
            }

            ImGui::SameLine();
        }

        ImGui::NewLine();
    }

    bool DataPropertiesDialog::ApplyChanges(DataObject *dataObject)
    {

        /*
        try {
            spdlog::info("Applying changes to {} '{}'", state.pService->GetName());

            // Get the startup type
            DWORD startType = GetStartupTypeFromIndex(state.startupType);

            // Call ServiceManager to update the service configuration
            ServiceManager::ChangeServiceConfig(
                state.pService->GetName(),
                state.displayName,
                state.description,
                startType,
                state.binaryPathName
            );

            // Update the local service object with new values
            state.pService->SetDisplayName(state.displayName);
            state.pService->SetDescription(state.description);
            state.pService->SetStartType(startType);
            state.pService->SetBinaryPathName(state.binaryPathName);

            spdlog::info("Service '{}' configuration updated successfully", state.pService->GetName());
            return true;

        }
        catch (const std::exception& e) {
            spdlog::error("Failed to update service configuration: {}", e.what());
            return false;
        }
        */
        return false;
    }

} // namespace pserv
