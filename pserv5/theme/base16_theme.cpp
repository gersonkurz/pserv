#include "precomp.h"
#include "theme/base16_theme.h"

// fkYAML is only used in this translation unit. C5311 (deprecated literal-
// operator-id form) originates inside fkYAML's user-defined literals, not our
// code, so suppress it locally.
#pragma warning(push)
#pragma warning(disable : 5311)
#include <fkYAML/node.hpp>
#pragma warning(pop)

#include "utils/string_utils.h"

namespace pserv::theme
{
    namespace
    {
        // --- File I/O -------------------------------------------------------

        std::string ReadAll(const std::wstring& path)
        {
            const HANDLE h{CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
            if (h == INVALID_HANDLE_VALUE)
                return {};

            LARGE_INTEGER size{};
            std::string out;
            if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < (16 << 20))
            {
                out.resize(static_cast<size_t>(size.QuadPart));
                DWORD read{0};
                if (!ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr))
                    out.clear();
                else
                    out.resize(read);
            }
            CloseHandle(h);
            return out;
        }

        // --- Color helpers --------------------------------------------------

        ImVec4 HexToColor(const std::string& s)
        {
            // Accepts "#RRGGBB". Returns opaque black on malformed input.
            const auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            if (s.size() == 7 && s[0] == '#')
            {
                int v[6]{};
                bool ok{true};
                for (int i{0}; i < 6; ++i)
                {
                    v[i] = hex(s[static_cast<size_t>(i) + 1]);
                    if (v[i] < 0) { ok = false; break; }
                }
                if (ok)
                {
                    return ImVec4{
                        (v[0] * 16 + v[1]) / 255.0f,
                        (v[2] * 16 + v[3]) / 255.0f,
                        (v[4] * 16 + v[5]) / 255.0f,
                        1.0f};
                }
            }
            return ImVec4{0.0f, 0.0f, 0.0f, 1.0f};
        }

        float Luminance(const ImVec4& c)
        {
            return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
        }

        ImVec4 Lerp(const ImVec4& a, const ImVec4& b, float t)
        {
            return ImVec4{
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t,
                a.w + (b.w - a.w) * t};
        }

        ImVec4 WithAlpha(const ImVec4& c, float a)
        {
            return ImVec4{c.x, c.y, c.z, a};
        }

        // --- YAML parsing ---------------------------------------------------

        bool LoadYamlScheme(const std::wstring& path, Base16Theme& out)
        {
            const std::string content{ReadAll(path)};
            if (content.empty())
                return false;

            try
            {
                auto root{fkyaml::node::deserialize(content)};

                if (!root.contains("name") || !root.contains("palette"))
                    return false;

                const std::string name{root["name"].get_value<std::string>()};
                if (name.empty())
                    return false;

                auto& palette{root["palette"]};
                static const char* keys[16]{
                    "base00", "base01", "base02", "base03",
                    "base04", "base05", "base06", "base07",
                    "base08", "base09", "base0A", "base0B",
                    "base0C", "base0D", "base0E", "base0F"};

                ImVec4 colors[16]{};
                for (int i{0}; i < 16; ++i)
                {
                    if (!palette.contains(keys[i]))
                        return false;
                    colors[i] = HexToColor(palette[keys[i]].get_value<std::string>());
                }

                out.name = name;
                for (int i{0}; i < 16; ++i)
                    out.base[i] = colors[i];
                out.isDark = Luminance(colors[0]) < 0.5f;
                return true;
            }
            catch (const fkyaml::exception& e)
            {
                spdlog::error("Failed to parse YAML theme '{}': {}",
                              pserv::utils::WideToUtf8(path), e.what());
                return false;
            }
        }

        // --- Built-in fallback (Catppuccin Mocha) ---------------------------

        Base16Theme FallbackScheme()
        {
            const char* mocha[16]{
                "#1e1e2e", "#181825", "#313244", "#45475a",
                "#585b70", "#cdd6f4", "#f5e0dc", "#b4befe",
                "#f38ba8", "#fab387", "#f9e2af", "#a6e3a1",
                "#94e2d5", "#89b4fa", "#cba6f7", "#f2cdcd"};
            Base16Theme t;
            t.name = "Catppuccin Mocha";
            for (int i{0}; i < 16; ++i)
                t.base[i] = HexToColor(mocha[i]);
            t.isDark = Luminance(t.base[0]) < 0.5f;
            return t;
        }
    } // namespace

    // --- Base16Theme --------------------------------------------------------

    ImVec4 Base16Theme::AccentText() const
    {
        // Contrast against the accent fill: dark accent -> white, light -> base00.
        return Luminance(base[0x0D]) > 0.5f ? base[0x00] : ImVec4{1.0f, 1.0f, 1.0f, 1.0f};
    }

    // --- ThemeSet -----------------------------------------------------------

    void ThemeSet::LoadFromDirectory(const std::wstring& themesDir)
    {
        m_themes.clear();
        m_current = 0;

        const std::wstring pattern{themesDir + L"\\*.yaml"};
        WIN32_FIND_DATAW fd{};
        const HANDLE hFind{FindFirstFileW(pattern.c_str(), &fd)};
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    continue;
                const std::wstring filePath{themesDir + L"\\" + fd.cFileName};
                Base16Theme t;
                if (LoadYamlScheme(filePath, t))
                    m_themes.push_back(std::move(t));
            } while (FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }

        std::sort(m_themes.begin(), m_themes.end(),
                  [](const Base16Theme& a, const Base16Theme& b) { return a.name < b.name; });

        if (m_themes.empty())
        {
            spdlog::warn("No themes loaded from '{}'; using built-in fallback",
                         pserv::utils::WideToUtf8(themesDir));
            m_themes.push_back(FallbackScheme());
        }

        spdlog::info("Loaded {} theme(s) from '{}'", m_themes.size(),
                     pserv::utils::WideToUtf8(themesDir));
    }

    bool ThemeSet::SelectByName(const std::string& name)
    {
        for (size_t i{0}; i < m_themes.size(); ++i)
        {
            if (m_themes[i].name == name)
            {
                m_current = i;
                return true;
            }
        }
        return false;
    }

    std::vector<ThemeInfo> ThemeSet::Themes() const
    {
        std::vector<ThemeInfo> infos;
        infos.reserve(m_themes.size());
        for (const auto& t : m_themes)
            infos.push_back(ThemeInfo{t.name, t.isDark});
        return infos;
    }

    // --- Base16 -> ImGuiStyle mapping ---------------------------------------

    void ApplyToImGui(const Base16Theme& t, ImGuiStyle& style)
    {
        const ImVec4* b = t.base;
        const ImVec4 accent = b[0x0D];
        const ImVec4 accentHover = Lerp(accent, b[0x05], 0.20f);
        const ImVec4 accentActive = Lerp(accent, b[0x00], 0.20f);

        ImVec4* c = style.Colors;

        c[ImGuiCol_Text]                  = b[0x05];
        c[ImGuiCol_TextDisabled]          = b[0x03];
        c[ImGuiCol_WindowBg]              = b[0x00];
        c[ImGuiCol_ChildBg]               = b[0x00];
        c[ImGuiCol_PopupBg]               = b[0x01];
        c[ImGuiCol_Border]                = b[0x02];
        c[ImGuiCol_BorderShadow]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        c[ImGuiCol_FrameBg]               = b[0x01];
        c[ImGuiCol_FrameBgHovered]        = Lerp(b[0x01], b[0x02], 0.5f);
        c[ImGuiCol_FrameBgActive]         = b[0x02];

        c[ImGuiCol_TitleBg]               = b[0x01];
        c[ImGuiCol_TitleBgActive]         = b[0x01];
        c[ImGuiCol_TitleBgCollapsed]      = b[0x00];
        c[ImGuiCol_MenuBarBg]             = b[0x01];

        c[ImGuiCol_ScrollbarBg]           = b[0x00];
        c[ImGuiCol_ScrollbarGrab]         = b[0x02];
        c[ImGuiCol_ScrollbarGrabHovered]  = b[0x03];
        c[ImGuiCol_ScrollbarGrabActive]   = Lerp(b[0x03], b[0x04], 0.5f);

        c[ImGuiCol_CheckMark]             = accent;
        c[ImGuiCol_SliderGrab]            = accent;
        c[ImGuiCol_SliderGrabActive]      = accentActive;

        c[ImGuiCol_Button]                = b[0x02];
        c[ImGuiCol_ButtonHovered]         = Lerp(b[0x02], accent, 0.5f);
        c[ImGuiCol_ButtonActive]          = accent;

        c[ImGuiCol_Header]                = accent;
        c[ImGuiCol_HeaderHovered]         = accentHover;
        c[ImGuiCol_HeaderActive]          = accentActive;

        c[ImGuiCol_Separator]             = b[0x02];
        c[ImGuiCol_SeparatorHovered]      = accentHover;
        c[ImGuiCol_SeparatorActive]       = accent;

        c[ImGuiCol_ResizeGrip]            = WithAlpha(b[0x02], 0.0f);
        c[ImGuiCol_ResizeGripHovered]     = accentHover;
        c[ImGuiCol_ResizeGripActive]      = accent;

        c[ImGuiCol_Tab]                   = b[0x01];
        c[ImGuiCol_TabHovered]            = accentHover;
        c[ImGuiCol_TabActive]             = accent;
        c[ImGuiCol_TabUnfocused]          = b[0x01];
        c[ImGuiCol_TabUnfocusedActive]    = Lerp(b[0x01], accent, 0.4f);

        c[ImGuiCol_PlotLines]             = accent;
        c[ImGuiCol_PlotLinesHovered]      = accentHover;
        c[ImGuiCol_PlotHistogram]         = accent;
        c[ImGuiCol_PlotHistogramHovered]  = accentHover;

        c[ImGuiCol_TableHeaderBg]         = b[0x01];
        c[ImGuiCol_TableBorderStrong]     = b[0x02];
        c[ImGuiCol_TableBorderLight]      = Lerp(b[0x01], b[0x02], 0.5f);
        c[ImGuiCol_TableRowBg]            = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        c[ImGuiCol_TableRowBgAlt]         = WithAlpha(b[0x01], 0.4f);

        c[ImGuiCol_TextSelectedBg]        = WithAlpha(accent, 0.35f);
        c[ImGuiCol_DragDropTarget]        = b[0x0A];
        c[ImGuiCol_NavHighlight]          = accent;
        c[ImGuiCol_NavWindowingHighlight] = WithAlpha(b[0x07], 0.7f);
        c[ImGuiCol_NavWindowingDimBg]     = WithAlpha(b[0x00], 0.2f);
        c[ImGuiCol_ModalWindowDimBg]      = WithAlpha(b[0x00], 0.5f);
    }
}
