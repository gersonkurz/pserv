#pragma once
#include <string>
#include <cstdint>
#include <format>

namespace pserv::utils {

// Formats bytes into human-readable size (KB, MB, GB, etc.)
// Returns empty string if bytes == 0
inline std::string FormatSize(uint64_t bytes) {
    if (bytes == 0) {
        return "";
    }

    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    // Format with 2 decimal places for KB and above, no decimals for bytes
    if (unitIndex == 0) {
        return std::format("{:.0f} {}", size, units[unitIndex]);
    }
    return std::format("{:.2f} {}", size, units[unitIndex]);
}

} // namespace pserv::utils
