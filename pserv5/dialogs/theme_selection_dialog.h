/// @file theme_selection_dialog.h
/// @brief Modal-style picker for Base16 themes.
///
/// Lists every loaded theme grouped into Dark / Light sections. Selection is
/// immediate: choosing a theme invokes a callback so the host can apply and
/// persist it (live preview). The host owns the ThemeSet; this dialog only
/// borrows a reference.
#pragma once

#include <functional>
#include "theme/base16_theme.h"

namespace pserv
{
    /// @brief ImGui theme picker. Render() must be called every frame while open.
    class ThemeSelectionDialog final
    {
    public:
        /// @brief Invoked with the chosen theme name the moment a row is picked.
        using SelectCallback = std::function<void(const std::string&)>;

        ThemeSelectionDialog(theme::ThemeSet& themes, SelectCallback onSelect);
        ~ThemeSelectionDialog() = default;

        void Open();
        void Close() { m_bOpen = false; }
        bool IsOpen() const { return m_bOpen; }

        /// @brief Render the picker window (no-op when closed).
        void Render();

    private:
        theme::ThemeSet& m_themes;
        SelectCallback m_onSelect;
        bool m_bOpen{false};
    };

} // namespace pserv
