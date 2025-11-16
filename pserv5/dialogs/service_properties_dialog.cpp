#include "precomp.h"
#include "service_properties_dialog.h"
#include "../windows_api/service_manager.h"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <format>

namespace pserv {

void ServicePropertiesDialog::Open(ServiceInfo* service) {
    if (!service) return;

    m_pService = service;
    m_bOpen = true;
    m_bDirty = false;

    InitializeFields();
}

void ServicePropertiesDialog::Close() {
    m_bOpen = false;
    m_pService = nullptr;
    m_bDirty = false;
}

void ServicePropertiesDialog::InitializeFields() {
    if (!m_pService) return;

    // Copy current values to edit buffers
    strncpy_s(m_displayName, m_pService->GetDisplayName().c_str(), sizeof(m_displayName) - 1);
    strncpy_s(m_description, m_pService->GetDescription().c_str(), sizeof(m_description) - 1);
    strncpy_s(m_binaryPathName, m_pService->GetBinaryPathName().c_str(), sizeof(m_binaryPathName) - 1);
    m_startupType = GetStartupTypeIndex(m_pService->GetStartType());
}

int ServicePropertiesDialog::GetStartupTypeIndex(DWORD startType) const {
    switch (startType) {
    case SERVICE_AUTO_START:
        return 0;  // Automatic
    case SERVICE_DEMAND_START:
        return 1;  // Manual
    case SERVICE_DISABLED:
        return 2;  // Disabled
    case SERVICE_BOOT_START:
        return 3;  // Boot
    case SERVICE_SYSTEM_START:
        return 4;  // System
    default:
        return 1;  // Default to Manual
    }
}

DWORD ServicePropertiesDialog::GetStartupTypeFromIndex(int index) const {
    switch (index) {
    case 0: return SERVICE_AUTO_START;
    case 1: return SERVICE_DEMAND_START;
    case 2: return SERVICE_DISABLED;
    case 3: return SERVICE_BOOT_START;
    case 4: return SERVICE_SYSTEM_START;
    default: return SERVICE_DEMAND_START;
    }
}

bool ServicePropertiesDialog::Render() {
    if (!m_bOpen || !m_pService) {
        return false;
    }

    bool changesApplied = false;

    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(std::format("Service Properties: {}", m_pService->GetName()).c_str(), &m_bOpen, ImGuiWindowFlags_NoCollapse)) {

        // === GENERAL SECTION ===
        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Service Name:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", m_pService->GetName().c_str());

            ImGui::Separator();

            ImGui::Text("Display Name:");
            ImGui::SameLine(200);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##displayname", m_displayName, sizeof(m_displayName))) {
                m_bDirty = true;
            }

            ImGui::Separator();

            ImGui::Text("Description:");
            ImGui::SameLine(200);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputTextMultiline("##description", m_description, sizeof(m_description), ImVec2(-1, 60))) {
                m_bDirty = true;
            }

            ImGui::Separator();

            ImGui::Text("Status:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", m_pService->GetStatusString().c_str());
        }

        // === CONFIGURATION SECTION ===
        if (ImGui::CollapsingHeader("Configuration", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Startup Type:");
            ImGui::SameLine(200);
            ImGui::SetNextItemWidth(-1);
            const char* startupTypes[] = { "Automatic", "Manual", "Disabled", "Boot", "System" };
            if (ImGui::Combo("##startuptype", &m_startupType, startupTypes, IM_ARRAYSIZE(startupTypes))) {
                m_bDirty = true;
            }

            ImGui::Separator();

            ImGui::Text("Service Type:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", m_pService->GetServiceTypeString().c_str());

            ImGui::Separator();

            ImGui::Text("Error Control:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", m_pService->GetErrorControlString().c_str());

            ImGui::Separator();

            ImGui::Text("User Account:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", m_pService->GetUser().c_str());

            ImGui::Separator();

            ImGui::Text("Binary Path:");
            ImGui::SameLine(200);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##binarypath", m_binaryPathName, sizeof(m_binaryPathName))) {
                m_bDirty = true;
            }
        }

        // === DETAILS SECTION ===
        if (ImGui::CollapsingHeader("Details")) {
            ImGui::Text("Process ID:");
            ImGui::SameLine(200);
            if (m_pService->GetProcessId() > 0) {
                ImGui::Text("%u", m_pService->GetProcessId());
            } else {
                ImGui::TextDisabled("N/A");
            }

            ImGui::Separator();

            ImGui::Text("Win32 Exit Code:");
            ImGui::SameLine(200);
            ImGui::Text("%u", m_pService->GetWin32ExitCode());

            ImGui::Separator();

            ImGui::Text("Service Exit Code:");
            ImGui::SameLine(200);
            ImGui::Text("%u", m_pService->GetServiceSpecificExitCode());

            ImGui::Separator();

            ImGui::Text("Checkpoint:");
            ImGui::SameLine(200);
            ImGui::Text("%u", m_pService->GetCheckPoint());

            ImGui::Separator();

            ImGui::Text("Wait Hint:");
            ImGui::SameLine(200);
            ImGui::Text("%u ms", m_pService->GetWaitHint());

            ImGui::Separator();

            ImGui::Text("Service Flags:");
            ImGui::SameLine(200);
            ImGui::Text("%u", m_pService->GetServiceFlags());

            ImGui::Separator();

            ImGui::Text("Controls Accepted:");
            ImGui::SameLine(200);
            ImGui::TextWrapped("%s", m_pService->GetControlsAcceptedString().c_str());
        }

        // === DEPENDENCIES SECTION ===
        if (ImGui::CollapsingHeader("Dependencies")) {
            ImGui::Text("Load Order Group:");
            ImGui::SameLine(200);
            std::string loadOrderGroup = m_pService->GetLoadOrderGroup();
            if (!loadOrderGroup.empty()) {
                ImGui::TextWrapped("%s", loadOrderGroup.c_str());
            } else {
                ImGui::TextDisabled("None");
            }

            ImGui::Separator();

            ImGui::Text("Tag ID:");
            ImGui::SameLine(200);
            if (m_pService->GetTagId() > 0) {
                ImGui::Text("%u", m_pService->GetTagId());
            } else {
                ImGui::TextDisabled("None");
            }
        }

        ImGui::Separator();

        // === BUTTONS ===
        ImGui::Spacing();
        if (ImGui::Button("Apply", ImVec2(100, 0))) {
            if (m_bDirty) {
                if (ApplyChanges()) {
                    changesApplied = true;
                    m_bDirty = false;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("OK", ImVec2(100, 0))) {
            if (m_bDirty) {
                if (ApplyChanges()) {
                    changesApplied = true;
                }
            }
            Close();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            Close();
        }
    }
    ImGui::End();

    if (!m_bOpen) {
        m_pService = nullptr;
    }

    return changesApplied;
}

bool ServicePropertiesDialog::ApplyChanges() {
    if (!m_pService) return false;

    try {
        spdlog::info("Applying changes to service '{}'", m_pService->GetName());

        // Get the startup type
        DWORD startType = GetStartupTypeFromIndex(m_startupType);

        // Call ServiceManager to update the service configuration
        ServiceManager::ChangeServiceConfig(
            m_pService->GetName(),
            m_displayName,
            m_description,
            startType,
            m_binaryPathName
        );

        // Update the local service object with new values
        m_pService->SetDisplayName(m_displayName);
        m_pService->SetDescription(m_description);
        m_pService->SetStartType(startType);
        m_pService->SetBinaryPathName(m_binaryPathName);

        spdlog::info("Service '{}' configuration updated successfully", m_pService->GetName());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to update service configuration: {}", e.what());
        return false;
    }
}

} // namespace pserv
