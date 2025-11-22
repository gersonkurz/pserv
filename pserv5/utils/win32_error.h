#pragma once

namespace pserv::utils
{

    /// Get formatted error message for a Win32 error code
    inline std::string GetWin32ErrorMessage(DWORD errorCode)
    {
        if (errorCode == 0)
        {
            return "Success";
        }

        LPWSTR messageBuffer{nullptr};
        size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&messageBuffer),
            0,
            nullptr);

        std::string message;
        if (size > 0)
        {
            // Convert wide string to UTF-8
            int utf8Size = WideCharToMultiByte(CP_UTF8, 0, messageBuffer, static_cast<int>(size), nullptr, 0, nullptr, nullptr);
            if (utf8Size > 0)
            {
                message.resize(utf8Size);
                WideCharToMultiByte(CP_UTF8, 0, messageBuffer, static_cast<int>(size), message.data(), utf8Size, nullptr, nullptr);

                // Trim trailing whitespace/newlines
                while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' '))
                {
                    message.pop_back();
                }
            }
        }

        LocalFree(messageBuffer);

        if (message.empty())
        {
            return std::format("Unknown error 0x{:08X}", errorCode);
        }

        return std::format("{} (0x{:08X})", message, errorCode);
    }

    /// Get formatted error message for the last Win32 error
    inline std::string GetLastWin32ErrorMessage()
    {
        return GetWin32ErrorMessage(GetLastError());
    }

    /// Log a Win32 API failure without additional message (internal implementation)
    inline void LogWin32ErrorImpl(const char *file, int line, const char *apiName)
    {
        spdlog::error("{}:{}: {} failed: {}", file, line, apiName, GetLastWin32ErrorMessage());
    }

    /// Log a Win32 API failure with additional formatted message (internal implementation)
    template <typename... Args> inline void LogWin32ErrorImpl(const char *file, int line, const char *apiName, std::format_string<Args...> fmt, Args &&...args)
    {
        std::string userMessage = std::format(fmt, std::forward<Args>(args)...);
        spdlog::error("{}:{}: {} failed: {} - {}", file, line, apiName, GetLastWin32ErrorMessage(), userMessage);
    }

    /// Log an expected Win32 API failure without additional message (internal implementation)
    inline void LogExpectedWin32ErrorImpl(const char *file, int line, const char *apiName)
    {
        spdlog::debug("{}:{}: {} failed: {}", file, line, apiName, GetLastWin32ErrorMessage());
    }

    /// Log an expected Win32 API failure with additional formatted message (internal implementation)
    template <typename... Args> inline void LogExpectedWin32ErrorImpl(const char *file, int line, const char *apiName, std::format_string<Args...> fmt, Args &&...args)
    {
        std::string userMessage = std::format(fmt, std::forward<Args>(args)...);
        spdlog::debug("{}:{}: {} failed: {} - {}", file, line, apiName, GetLastWin32ErrorMessage(), userMessage);
    }

    /// Log a Win32 API failure with explicit error code without additional message (internal implementation)
    inline void LogWin32ErrorCodeImpl(const char *file, int line, const char *apiName, DWORD errorCode)
    {
        spdlog::error("{}:{}: {} failed: {}", file, line, apiName, GetWin32ErrorMessage(errorCode));
    }

    /// Log a Win32 API failure with explicit error code and additional formatted message (internal implementation)
    template <typename... Args>
    inline void LogWin32ErrorCodeImpl(const char *file, int line, const char *apiName, DWORD errorCode, std::format_string<Args...> fmt, Args &&...args)
    {
        std::string userMessage = std::format(fmt, std::forward<Args>(args)...);
        spdlog::error("{}:{}: {} failed: {} - {}", file, line, apiName, GetWin32ErrorMessage(errorCode), userMessage);
    }

    /// Log an expected Win32 API failure with explicit error code without additional message (internal implementation)
    inline void LogExpectedWin32ErrorCodeImpl(const char *file, int line, const char *apiName, DWORD errorCode)
    {
        spdlog::debug("{}:{}: {} failed: {}", file, line, apiName, GetWin32ErrorMessage(errorCode));
    }

    /// Log an expected Win32 API failure with explicit error code and additional formatted message (internal implementation)
    template <typename... Args>
    inline void LogExpectedWin32ErrorCodeImpl(const char *file, int line, const char *apiName, DWORD errorCode, std::format_string<Args...> fmt, Args &&...args)
    {
        std::string userMessage = std::format(fmt, std::forward<Args>(args)...);
        spdlog::debug("{}:{}: {} failed: {} - {}", file, line, apiName, GetWin32ErrorMessage(errorCode), userMessage);
    }

} // namespace pserv::utils

// Macro to capture file/line automatically - supports both with and without additional message
#define LogWin32Error(apiName, ...) ::pserv::utils::LogWin32ErrorImpl(__FILE__, __LINE__, apiName, ##__VA_ARGS__)

// Macro for expected/common Win32 failures that should be logged at debug level
#define LogExpectedWin32Error(apiName, ...) ::pserv::utils::LogExpectedWin32ErrorImpl(__FILE__, __LINE__, apiName, ##__VA_ARGS__)

// Macro for APIs that return error codes directly (like registry functions) - requires explicit error code parameter
#define LogWin32ErrorCode(apiName, errorCode, ...) ::pserv::utils::LogWin32ErrorCodeImpl(__FILE__, __LINE__, apiName, errorCode, ##__VA_ARGS__)

// Macro for expected failures with explicit error code
#define LogExpectedWin32ErrorCode(apiName, errorCode, ...) ::pserv::utils::LogExpectedWin32ErrorCodeImpl(__FILE__, __LINE__, apiName, errorCode, ##__VA_ARGS__)
