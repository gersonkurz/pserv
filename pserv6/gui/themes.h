#pragma once
#include <array>
#include <charconv>
#include <regex>
#include <wil/win32_helpers.h>

namespace pserv6
{
    /// A Base16 colour scheme read from themes\*.yaml next to pserv6.exe.
    struct Base16Theme
    {
        std::string name; ///< Display name; also the value of the application.theme setting
        bool bDark{true};
        std::array<winrt::Windows::UI::Color, 16> base{};
    };

    /// Every well-formed theme in <exe folder>\themes, sorted by name. A file that lacks a name or
    /// one of the 16 colours is skipped with a warning.
    inline std::vector<Base16Theme> LoadBase16Themes()
    {
        std::vector<Base16Theme> themes;
        const auto folder = std::filesystem::path{wil::GetModuleFileNameW().get()}.parent_path() / L"themes";
        // ponytail: reads flat `key: "value"` lines, which is all the Base16 files contain;
        // a YAML library when a theme file needs more than that
        static const std::regex pattern{R"re(^\s*(\w+):\s*"([^"]*)")re"};
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator{folder, ec})
        {
            if (entry.path().extension() != L".yaml")
                continue;
            Base16Theme theme;
            unsigned found = 0;
            std::ifstream file{entry.path()};
            for (std::string line; std::getline(file, line);)
            {
                std::smatch match;
                if (!std::regex_search(line, match, pattern))
                    continue;
                const std::string key = match[1], value = match[2];
                unsigned index = 0, rgb = 0;
                if (key == "name")
                    theme.name = value;
                else if (key == "variant")
                    theme.bDark = value != "light";
                else if (key.size() == 6 && key.starts_with("base0") && value.size() == 7 && value[0] == '#' &&
                         std::from_chars(key.data() + 5, key.data() + 6, index, 16).ec == std::errc{} &&
                         std::from_chars(value.data() + 1, value.data() + 7, rgb, 16).ptr == value.data() + 7)
                {
                    theme.base[index] = {255, static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb)};
                    found |= 1u << index;
                }
            }
            if (!theme.name.empty() && found == 0xFFFF)
                themes.push_back(std::move(theme));
            else
                spdlog::warn("Theme file {} skipped: no name or not all of base00..base0F", entry.path().filename().string());
        }
        std::ranges::sort(themes, {}, &Base16Theme::name);
        return themes;
    }
} // namespace pserv6
