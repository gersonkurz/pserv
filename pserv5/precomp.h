#pragma once

// Windows headers
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
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