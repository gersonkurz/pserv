#include "precomp.h"
#include "process_properties_dialog.h"
#include <utils/format_utils.h>
#include <imgui.h>
#include <format>

namespace pserv {

void ProcessPropertiesDialog::Open(const std::vector<ProcessInfo*>& processes) {
    if (processes.empty()) return;

    m_processStates.clear();
    m_activeTabIndex = 0;

    for (const auto* proc : processes) {
        if (!proc) continue;

        ProcessPropertiesState state;
        state.pOriginalProcess = proc;
        
        // Copy snapshot data
        state.name = proc->GetName();
        state.pid = proc->GetPid();
        state.parentPid = proc->GetParentPid();
        state.user = proc->GetUser();
        state.path = proc->GetPath();
        state.commandLine = proc->GetCommandLine();
        state.priority = proc->GetPriorityString();

        state.threadCount = std::to_string(proc->GetThreadCount());
        state.handleCount = std::to_string(proc->GetHandleCount());
        state.workingSet = utils::FormatSize(proc->GetWorkingSetSize());
        state.peakWorkingSet = utils::FormatSize(proc->GetPeakWorkingSetSize());
        state.privateBytes = utils::FormatSize(proc->GetPrivatePageCount());
        state.virtualSize = utils::FormatSize(proc->GetVirtualSize());
        
        // These might rely on the strings returned by GetProperty logic if we exposed getters
        // But since we have getters on ProcessInfo, we use them directly or helper
        // NOTE: We need to access the raw values for PagedPool etc if we didn't expose getters
        // Wait, I didn't expose getters for PagedPool/PageFaults in ProcessInfo header!
        // I only added SetMemoryExtras and GetProperty logic.
        // I should access via GetProperty to be consistent/lazy, or add getters.
        // Using GetProperty is easiest without modifying ProcessInfo.h again.
        
        state.pagedPool = proc->GetProperty(static_cast<int>(ProcessProperty::PagedPoolUsage));
        state.nonPagedPool = proc->GetProperty(static_cast<int>(ProcessProperty::NonPagedPoolUsage));
        state.pageFaults = proc->GetProperty(static_cast<int>(ProcessProperty::PageFaultCount));
        
        state.startTime = proc->GetProperty(static_cast<int>(ProcessProperty::StartTime));
        state.totalCpuTime = proc->GetProperty(static_cast<int>(ProcessProperty::TotalCPUTime));
        state.userCpuTime = proc->GetProperty(static_cast<int>(ProcessProperty::UserCPUTime));
        state.kernelCpuTime = proc->GetProperty(static_cast<int>(ProcessProperty::KernelCPUTime));

        m_processStates.push_back(state);
    }

    if (!m_processStates.empty()) {
        m_bOpen = true;
    }
}

void ProcessPropertiesDialog::Close() {
    m_bOpen = false;
    m_processStates.clear();
}

void ProcessPropertiesDialog::Render() {
    if (!m_bOpen || m_processStates.empty()) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500, 450), ImGuiCond_FirstUseEver);
    
    std::string windowTitle = m_processStates.size() == 1
        ? std::format("Process Properties: {}", m_processStates[0].name)
        : std::format("Process Properties ({} processes)", m_processStates.size());

    if (ImGui::Begin(windowTitle.c_str(), &m_bOpen, ImGuiWindowFlags_NoCollapse)) {
        
        if (m_processStates.size() > 1) {
            ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_FittingPolicyScroll;
            if (ImGui::BeginTabBar("ProcessTabs", tabBarFlags)) {
                for (size_t i = 0; i < m_processStates.size(); ++i) {
                    std::string tabLabel = std::format("{}###Proc{}", m_processStates[i].name, m_processStates[i].pid);
                    if (ImGui::BeginTabItem(tabLabel.c_str())) {
                        m_activeTabIndex = static_cast<int>(i);
                        RenderProcessContent(m_processStates[i]);
                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
        } else {
            RenderProcessContent(m_processStates[0]);
        }

        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            Close();
        }
    }
    ImGui::End();

    if (!m_bOpen) {
        m_processStates.clear();
    }
}

void ProcessPropertiesDialog::RenderProcessContent(const ProcessPropertiesState& state) {
    if (ImGui::BeginTabBar("DetailsTabs")) {
        
        // === GENERAL ===
        if (ImGui::BeginTabItem("General")) {
            ImGui::Spacing();
            
            // Name & Icon (placeholder for icon)
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", state.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(PID: %u)", state.pid);
            
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::Text("Command Line:");
            if (!state.commandLine.empty()) {
                std::vector<char> cmdLineBuf(state.commandLine.begin(), state.commandLine.end());
                cmdLineBuf.push_back(0);
                ImGui::InputTextMultiline("##cmdline", cmdLineBuf.data(), cmdLineBuf.size(), ImVec2(-1, 60), ImGuiInputTextFlags_ReadOnly);
            } else {
                char empty[] = "";
                ImGui::InputTextMultiline("##cmdline", empty, 1, ImVec2(-1, 60), ImGuiInputTextFlags_ReadOnly);
            }
            
            ImGui::Spacing();
            
            ImGui::Text("Path:");
            if (!state.path.empty()) {
                std::vector<char> pathBuf(state.path.begin(), state.path.end());
                pathBuf.push_back(0);
                ImGui::InputText("##path", pathBuf.data(), pathBuf.size(), ImGuiInputTextFlags_ReadOnly);
            } else {
                char empty[] = "";
                ImGui::InputText("##path", empty, 1, ImGuiInputTextFlags_ReadOnly);
            }
            
            ImGui::Spacing();
            
            if (ImGui::BeginTable("GeneralTable", 2)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Value");
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("User:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.user.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Parent PID:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%u", state.parentPid);
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Priority:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.priority.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Started:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.startTime.c_str());

                ImGui::EndTable();
            }
            
            ImGui::EndTabItem();
        }

        // === STATISTICS ===
        if (ImGui::BeginTabItem("Statistics")) {
            ImGui::Spacing();
            
            ImGui::TextDisabled("Performance & Memory");
            ImGui::Separator();
            
            if (ImGui::BeginTable("StatsTable", 2)) {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value");
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("CPU Time:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s (Kernel: %s, User: %s)", state.totalCpuTime.c_str(), state.kernelCpuTime.c_str(), state.userCpuTime.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Working Set:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s (Peak: %s)", state.workingSet.c_str(), state.peakWorkingSet.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Private Bytes:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.privateBytes.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Virtual Size:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.virtualSize.c_str());

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Paged Pool:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.pagedPool.c_str());

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Non-Paged Pool:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.nonPagedPool.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Page Faults:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.pageFaults.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Threads:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.threadCount.c_str());
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("Handles:");
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", state.handleCount.c_str());
                
                ImGui::EndTable();
            }
            
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
}

} // namespace pserv
