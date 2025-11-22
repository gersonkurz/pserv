#pragma once

namespace pserv::utils
{

    inline std::wstring Utf8ToWide(std::string_view utf8)
    {
        if (utf8.empty())
            return {};

        int size = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
        std::wstring result(size, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), size);
        return result;
    }

    inline std::string WideToUtf8(std::wstring_view wide)
    {
        if (wide.empty())
            return {};

        int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    inline std::string ToLower(std::string_view str)
    {
        std::string result(str);
        for (char &c : result)
        {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    inline bool ContainsIgnoreCase(const std::string& key, const std::string& lowerFilter)
    {
        return utils::ToLower(key).find(lowerFilter) != std::string::npos;
    }

} // namespace pserv::utils
