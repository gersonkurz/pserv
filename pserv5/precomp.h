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
#include <algorithm>

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <d3d11.h>
#include <wrl/client.h>
#include <dxgi.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>
#include <shellapi.h>

