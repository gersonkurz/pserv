#pragma once

// Windows headers
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

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

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
