/// @file string_utils.h
/// @brief String conversion and manipulation utilities.
///
/// Provides UTF-8/UTF-16 conversion, case conversion, and clipboard operations.
#pragma once

namespace pserv::utils
{
    /// @brief Convert UTF-8 string to UTF-16 wide string.
    inline std::wstring Utf8ToWide(std::string_view utf8)
    {
        if (utf8.empty())
            return {};

        int size = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring result(size, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), size);
        return result;
    }

    /// @brief Convert UTF-16 wide string to UTF-8 string.
    inline std::string WideToUtf8(std::wstring_view wide)
    {
        if (wide.empty())
            return {};

        int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    /// @brief Convert string to lowercase (ASCII only).
    inline std::string ToLower(std::string_view str)
    {
        std::string result(str);
        for (char &c : result)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    /// @brief Check if a string contains a substring (case-insensitive).
    /// @param key The string to search in.
    /// @param lowerFilter The substring to find (must be pre-lowercased).
    inline bool ContainsIgnoreCase(const std::string& key, const std::string& lowerFilter)
    {
        return utils::ToLower(key).find(lowerFilter) != std::string::npos;
    }

    /// @brief Copy text to the system clipboard.
    /// Uses Win32 API in console build, ImGui in GUI build.
    #ifdef PSERV_CONSOLE_BUILD
    void CopyToClipboard(const std::string &text);
    #else
    inline void CopyToClipboard(const std::string& text)
    {
        ImGui::SetClipboardText(text.c_str());
    }
    #endif

} // namespace pserv::utils
