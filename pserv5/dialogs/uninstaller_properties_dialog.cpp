#include "precomp.h"
#include "uninstaller_properties_dialog.h"
#include <imgui.h>
#include <format>
#include <string>
#include "../utils/string_utils.h"
#include <spdlog/spdlog.h>

namespace pserv {

void UninstallerPropertiesDialog::Open(const std::vector<InstalledProgramInfo*>& programs) {
    if (programs.empty()) {
        Close();
        return;
    }
    m_programs = programs;
    m_bIsOpen = true;
    ImGui::OpenPopup("Program Properties");
}

bool UninstallerPropertiesDialog::Render() {
    if (!m_bIsOpen) {
        return false;
    }

    bool changesApplied = false;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Program Properties", &m_bIsOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        if (m_programs.empty()) {
            ImGui::Text("No program selected.");
        } else if (m_programs.size() == 1) {
            RenderSingleProgramProperties(m_programs[0]);
        } else {
            ImGui::Text("%zu Programs Selected", m_programs.size());
            // For multiple selections, just show a summary or common properties if applicable
        }

        ImGui::Separator();
        if (ImGui::Button("Close")) {
            Close();
        }
        ImGui::EndPopup();
    }
    else {
        m_bIsOpen = false; // Popup closed by user or other means
    }
    return changesApplied;
}

void UninstallerPropertiesDialog::Close() {
    m_bIsOpen = false;
    m_programs.clear();
    ImGui::CloseCurrentPopup();
}

void UninstallerPropertiesDialog::RenderSingleProgramProperties(InstalledProgramInfo* program) {
    ImGui::Text("Display Name: %s", program->GetDisplayName().c_str());
    ImGui::Text("Version: %s", program->GetDisplayVersion().c_str());
    ImGui::Text("Publisher: %s", program->GetPublisher().c_str());
    ImGui::Text("Install Location: %s", program->GetInstallLocation().c_str());
    ImGui::Text("Uninstall String: %s", program->GetUninstallString().c_str());
    ImGui::Text("Install Date: %s", program->GetInstallDate().c_str());
    ImGui::Text("Estimated Size: %s", program->GetEstimatedSize().c_str());
    ImGui::Text("Comments: %s", program->GetComments().c_str());
    ImGui::Text("Help Link: %s", program->GetHelpLink().c_str());
    ImGui::Text("URL Info About: %s", program->GetUrlInfoAbout().c_str());
}

} // namespace pserv
