/// @file base16_theme.h
/// @brief Base16 color-scheme loading and mapping to Dear ImGui styles.
///
/// Loads Base16 YAML schemes from a themes/ directory beside the executable and
/// maps the 16-color palette deterministically onto an ImGuiStyle. Schemes are
/// pure data (no GPU/device resources), so loading and swapping at runtime is
/// cheap. Mirrors the design of the sibling "environ" project, retargeted from
/// Direct2D to ImGui.
#pragma once

#ifndef PSERV_WINUI_BUILD
#include <imgui.h>
#endif
#include <string>
#include <vector>

namespace pserv::theme
{
#ifdef PSERV_WINUI_BUILD
    struct PaletteColor
    {
        float x{}, y{}, z{}, w{};
    };
#else
    using PaletteColor = ImVec4;
#endif
    /// @brief A parsed Base16 scheme: name, light/dark classification, and the
    ///        16 palette entries (base00..base0F) as linear-ish ImGui colors.
    struct Base16Theme
    {
        std::string name;            ///< Display name (from YAML "name").
        bool isDark{true};           ///< Derived from base00 luminance.
        PaletteColor base[16]{};     ///< base00..base0F.

        /// @brief Accent color used for selection/active highlights (base0D).
        const PaletteColor& Accent() const { return base[0x0D]; }

        /// @brief Foreground text drawn on top of an Accent() fill.
        /// White for dark accents, near-black (base00) for light accents.
        PaletteColor AccentText() const;
    };

    /// @brief Name + dark/light flag, for grouped listings in the picker.
    struct ThemeInfo
    {
        std::string name;
        bool dark;
    };

    /// @brief Owns all schemes loaded from a themes/ directory. Falls back to a
    /// built-in scheme (Catppuccin Mocha) if the directory is empty or missing.
    class ThemeSet
    {
    public:
        /// @brief (Re)load every *.yaml in @p themesDir. Always leaves at least
        /// one scheme available (the fallback). Sorted by name.
        void LoadFromDirectory(const std::wstring& themesDir);

        /// @brief Make the named scheme current. Returns false if not found.
        bool SelectByName(const std::string& name);

        const Base16Theme& Current() const { return m_themes[m_current]; }
        size_t Count() const { return m_themes.size(); }
        std::vector<ThemeInfo> Themes() const; ///< names + dark/light, in load order

    private:
        std::vector<Base16Theme> m_themes;
        size_t m_current{0};
    };

    /// @brief Map a Base16 scheme onto an ImGuiStyle's color table in place.
    /// Geometry (sizes/rounding) is left untouched.
#ifndef PSERV_WINUI_BUILD
    void ApplyToImGui(const Base16Theme& theme, ImGuiStyle& style);
#endif
}
