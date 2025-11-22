#pragma once

// Windows headers
//#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <taskschd.h>
//#include <ws2tcpip.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// C++20 Standard Library
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <format>
#include <algorithm>
#include <ranges>
#include <span>
#include <atomic>
#include <future>
#include <thread>
#include <chrono>
#include <sstream>
#include <source_location>
#include <fstream>
#include <cstdint>
#include <mutex>
#include <variant>
#include <optional>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

#include <d3d11.h>
#include <wrl/client.h>
#include <dxgi.h>
#include <winreg.h>
#include <tlhelp32.h>
#include <sddl.h>
#include <psapi.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>
#include <shellapi.h>
#include <ShObjIdl.h>

#include <wil/resource.h>
#include <wil/com.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <toml++/toml.hpp>

#ifdef _DEBUG
#define DBG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define DBG_NEW new
#endif

// see http://stackoverflow.com/questions/5641427/how-to-make-preprocessor-generate-a-string-for-line-keyword
#define S(x) #x
#define S_(x) S(x)
#define S__LINE__ S_(__LINE__)

// see http://msdn.microsoft.com/de-de/library/b0084kay.aspx
#define FUNCTION_CONTEXT __FUNCTION__ "[" S__LINE__ "]"

/// This macro can be used on classes that should not enable a copy / move constructor / assignment operator
#define DECLARE_NON_COPYABLE(__CLASSNAME__) \
    __CLASSNAME__(const __CLASSNAME__ &) = delete; \
    __CLASSNAME__ &operator=(const __CLASSNAME__ &) = delete; \
    __CLASSNAME__(__CLASSNAME__ &&) = delete; \
    __CLASSNAME__ &operator=(__CLASSNAME__ &&) = delete;
