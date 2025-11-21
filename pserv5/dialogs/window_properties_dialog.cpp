#include "precomp.h"
#include <dialogs/window_properties_dialog.h>

namespace pserv {

void WindowPropertiesDialog::Open(const std::vector<WindowInfo*>& windows) {
    if (windows.empty()) return;

    m_windowStates.clear();
    m_activeTabIndex = 0;

    for (const auto* win : windows) {
        if (!win) continue;

        WindowPropertiesState state;
        state.pOriginalWindow = win;
        
        // Snapshot properties
        state.hwnd = win->GetHandle();
        state.id = win->GetProperty(static_cast<int>(WindowProperty::InternalID));
        state.title = win->GetTitle();
        state.className = win->GetClassName();
        state.style = win->GetProperty(static_cast<int>(WindowProperty::Style));
        state.exStyle = win->GetProperty(static_cast<int>(WindowProperty::ExStyle));
        state.size = win->GetProperty(static_cast<int>(WindowProperty::Size));
        state.position = win->GetProperty(static_cast<int>(WindowProperty::Position));
        state.processId = win->GetProperty(static_cast<int>(WindowProperty::ProcessID));
        state.threadId = win->GetProperty(static_cast<int>(WindowProperty::ThreadID));
        state.processName = win->GetProperty(static_cast<int>(WindowProperty::Process));
        state.windowId = win->GetProperty(static_cast<int>(WindowProperty::ID));

        m_windowStates.push_back(state);
    }

    if (!m_windowStates.empty()) {
        m_bOpen = true;
    }
}

void WindowPropertiesDialog::Close() {
    m_bOpen = false;
    m_windowStates.clear();
}

void WindowPropertiesDialog::Render() {
    if (!m_bOpen || m_windowStates.empty()) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
    
    std::string windowTitle = m_windowStates.size() == 1
        ? std::format("Window Properties: {}", m_windowStates[0].id)
        : std::format("Window Properties ({} windows)", m_windowStates.size());

    if (ImGui::Begin(windowTitle.c_str(), &m_bOpen, ImGuiWindowFlags_NoCollapse)) {
        
        if (m_windowStates.size() > 1) {
            ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_FittingPolicyScroll;
            if (ImGui::BeginTabBar("WindowTabs", tabBarFlags)) {
                for (size_t i = 0; i < m_windowStates.size(); ++i) {
                    // Use Handle as ID to ensure uniqueness
                    std::string tabLabel = std::format("{}###Win{:X}", 
                        m_windowStates[i].title.empty() ? m_windowStates[i].id : m_windowStates[i].title.substr(0, 15), 
                        reinterpret_cast<uintptr_t>(m_windowStates[i].hwnd));
                        
                    if (ImGui::BeginTabItem(tabLabel.c_str())) {
                        m_activeTabIndex = static_cast<int>(i);
                        RenderWindowContent(m_windowStates[i]);
                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
        } else {
            RenderWindowContent(m_windowStates[0]);
        }

        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            Close();
        }
    }
    ImGui::End();

    if (!m_bOpen) {
        m_windowStates.clear();
    }
}

void WindowPropertiesDialog::RenderWindowContent(const WindowPropertiesState& state) {
    if (ImGui::BeginTabBar("DetailsTabs")) {
        
        // === GENERAL ===
        if (ImGui::BeginTabItem("General")) {
            ImGui::Spacing();
            
            // Title
            ImGui::Text("Title:");
            if (!state.title.empty()) {
                std::vector<char> buf(state.title.begin(), state.title.end());
                buf.push_back(0);
                ImGui::InputTextMultiline("##title", buf.data(), buf.size(), ImVec2(-1, 60), ImGuiInputTextFlags_ReadOnly);
            } else {
                ImGui::TextDisabled("(No Title)");
            }
            
            ImGui::Spacing();
            
            if (ImGui::BeginTable("GeneralTable", 2)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Value");
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Handle:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.id.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Class:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.className.c_str());

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Control ID:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.windowId.c_str());
                
                ImGui::EndTable();
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::TextDisabled("Process Information");
            if (ImGui::BeginTable("ProcessTable", 2)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Value");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Process:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s (PID: %s)", state.processName.c_str(), state.processId.c_str());

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Thread ID:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.threadId.c_str());

                ImGui::EndTable();
            }

            ImGui::EndTabItem();
        }

        // === STYLES ===
        if (ImGui::BeginTabItem("Styles")) {
            ImGui::Spacing();
            
            if (ImGui::BeginTable("StylesTable", 2)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Value");
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Style:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.style.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("ExStyle:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.exStyle.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Rectangle:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s %s", state.position.c_str(), state.size.c_str());

                ImGui::EndTable();
            }
            
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
}

} // namespace pserv
