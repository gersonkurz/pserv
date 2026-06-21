#include "precomp.h"
#include "dialogs/theme_selection_dialog.h"

namespace pserv
{
    ThemeSelectionDialog::ThemeSelectionDialog(theme::ThemeSet& themes, SelectCallback onSelect)
        : m_themes{themes}, m_onSelect{std::move(onSelect)}
    {
    }

    void ThemeSelectionDialog::Open()
    {
        m_bOpen = true;
    }

    void ThemeSelectionDialog::Render()
    {
        if (!m_bOpen)
            return;

        const std::string current = m_themes.Current().name;
        const auto themes = m_themes.Themes();

        ImGui::SetNextWindowSize(ImVec2(360, 480), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Select Theme", &m_bOpen, ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                m_bOpen = false;

            // Footer reserves a button row; the list scrolls above it.
            const float footerH = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
            if (ImGui::BeginChild("ThemeList", ImVec2(0, -footerH), true))
            {
                // Two passes: Dark group first, then Light. A header precedes each
                // non-empty group.
                const auto renderGroup = [&](const char* title, bool dark) {
                    bool headerShown = false;
                    for (const auto& info : themes)
                    {
                        if (info.dark != dark)
                            continue;
                        if (!headerShown)
                        {
                            ImGui::SeparatorText(title);
                            headerShown = true;
                        }
                        const bool selected = (info.name == current);
                        if (ImGui::Selectable(info.name.c_str(), selected))
                        {
                            if (info.name != current && m_themes.SelectByName(info.name))
                                m_onSelect(info.name);
                        }
                        // Keep the active theme scrolled into view when first opened.
                        if (selected && ImGui::IsWindowAppearing())
                            ImGui::SetScrollHereY(0.5f);
                    }
                };
                renderGroup("Dark", true);
                renderGroup("Light", false);
            }
            ImGui::EndChild();

            if (ImGui::Button("Close"))
                m_bOpen = false;
        }
        ImGui::End();
    }

} // namespace pserv
