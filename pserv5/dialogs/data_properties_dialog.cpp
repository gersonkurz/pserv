#include "precomp.h"
#include <core/data_action.h>
#include <core/data_controller.h>
#include <core/data_object.h>
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
            return;

        m_activeTabIndex = 0;

        for (auto dataObject : m_dataObjects)
        {
            assert(dataObject != nullptr);
            InitializeFields(dataObject);
        }

        if (!m_dataObjects.empty())
        {
            m_bOpen = true;
        }
    }

    void DataPropertiesDialog::Close()
    {
        m_bOpen = false;
        m_activeTabIndex = 0;
    }

    void DataPropertiesDialog::InitializeFields(const DataObject *dataObject)
    {
        /*
        // Copy current values to edit buffers
        strncpy_s(state.displayName, state.pService->GetDisplayName().c_str(), sizeof(state.displayName) - 1);
        strncpy_s(state.description, state.pService->GetDescription().c_str(), sizeof(state.description) - 1);
        strncpy_s(state.binaryPathName, state.pService->GetBinaryPathName().c_str(), sizeof(state.binaryPathName) - 1);
        state.startupType = GetStartupTypeIndex(state.pService->GetStartType());
        state.bDirty = false;
        */
    }

    bool DataPropertiesDialog::Render()
    {
        if (m_dataObjects.empty())
        {
            return false;
        }

        bool changesApplied = false;

        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        std::string windowTitle = m_dataObjects.size() == 1 ? std::format("{} Properties", m_controller->GetItemName())
                                                            : std::format("{} Properties ({} services)", m_controller->GetItemName(), m_dataObjects.size());

        bool m_bOpen = true;
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
                Close();
            }
        }
        ImGui::End();

        return changesApplied;
    }

    void DataPropertiesDialog::RenderContent(const DataObject *dataObject)
    {
        /*
        // === GENERAL SECTION ===
        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Service Name:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", state.pService->GetName().c_str());

            ImGui::Separator();

            ImGui::Text("Display Name:");
            ImGui::SameLine(200);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##displayname", state.displayName, sizeof(state.displayName))) {
                state.bDirty = true;
            }

            ImGui::Separator();

            ImGui::Text("Description:");
            ImGui::SameLine(200);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputTextMultiline("##description", state.description, sizeof(state.description), ImVec2(-1, 60))) {
                state.bDirty = true;
            }

            ImGui::Separator();

            ImGui::Text("Status:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", state.pService->GetStatusString().c_str());
        }

        // === CONFIGURATION SECTION ===
        if (ImGui::CollapsingHeader("Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Startup Type:");
            ImGui::SameLine(200);
            ImGui::SetNextItemWidth(-1);
            const char* startupTypes[] = { "Automatic", "Manual", "Disabled", "Boot", "System" };
            if (ImGui::Combo("##startuptype", &state.startupType, startupTypes, IM_ARRAYSIZE(startupTypes))) {
                state.bDirty = true;
            }

            ImGui::Separator();

            ImGui::Text("Service Type:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", state.pService->GetServiceTypeString().c_str());

            ImGui::Separator();

            ImGui::Text("Error Control:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", state.pService->GetErrorControlString().c_str());

            ImGui::Separator();

            ImGui::Text("User Account:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", state.pService->GetUser().c_str());

            ImGui::Separator();

            ImGui::Text("Binary Path:");
            ImGui::SameLine(200);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##binarypath", state.binaryPathName, sizeof(state.binaryPathName))) {
                state.bDirty = true;
            }
        }

        // === DETAILS SECTION ===
        if (ImGui::CollapsingHeader("Details")) {
            ImGui::Text("Process ID:");
            ImGui::SameLine(200);
            if (state.pService->GetProcessId() > 0) {
                ImGui::Text("%u", state.pService->GetProcessId());
            }
            else {
                ImGui::TextDisabled("N/A");
            }

            ImGui::Separator();

            ImGui::Text("Win32 Exit Code:");
            ImGui::SameLine(200);
            ImGui::Text("%u", state.pService->GetWin32ExitCode());

            ImGui::Separator();

            ImGui::Text("Service Exit Code:");
            ImGui::SameLine(200);
            ImGui::Text("%u", state.pService->GetServiceSpecificExitCode());

            ImGui::Separator();

            ImGui::Text("Checkpoint:");
            ImGui::SameLine(200);
            ImGui::Text("%u", state.pService->GetCheckPoint());

            ImGui::Separator();

            ImGui::Text("Wait Hint:");
            ImGui::SameLine(200);
            ImGui::Text("%u ms", state.pService->GetWaitHint());

            ImGui::Separator();

            ImGui::Text("Service Flags:");
            ImGui::SameLine(200);
            ImGui::Text("%u", state.pService->GetServiceFlags());

            ImGui::Separator();

            ImGui::Text("Controls Accepted:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", state.pService->GetControlsAcceptedString().c_str());
        }

        // === DEPENDENCIES SECTION ===
        if (ImGui::CollapsingHeader("Dependencies")) {
            ImGui::Text("Load Order Group:");
            ImGui::SameLine(200);
            std::string loadOrderGroup = state.pService->GetLoadOrderGroup();
            if (!loadOrderGroup.empty()) {
                ImGui::TextWrapped("%s", loadOrderGroup.c_str());
            }
            else {
                ImGui::TextDisabled("None");
            }

            ImGui::Separator();

            ImGui::Text("Tag ID:");
            ImGui::SameLine(200);
            if (state.pService->GetTagId() > 0) {
                ImGui::Text("%u", state.pService->GetTagId());
            }
            else {
                ImGui::TextDisabled("None");
            }
        }
            */
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
