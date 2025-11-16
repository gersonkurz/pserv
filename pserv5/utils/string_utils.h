#pragma once
#include <string>
#include <string_view>
#include <Windows.h>

namespace pserv::utils {

inline std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};

    int size = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                     static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                         static_cast<int>(utf8.size()), result.data(), size);
    return result;
}

inline std::string WideToUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};

    int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                     static_cast<int>(wide.size()),
                                     nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                         static_cast<int>(wide.size()),
                         result.data(), size, nullptr, nullptr);
    return result;
}

} // namespace pserv::utils
