#include "precomp.h"
#include "window_info.h"
#include "../utils/string_utils.h"

namespace pserv {

WindowInfo::WindowInfo(HWND hwnd)
    : m_hwnd{ hwnd }
{
}

PropertyValue WindowInfo::GetTypedProperty(int propertyId) const {
    switch (static_cast<WindowProperty>(propertyId)) {
        case WindowProperty::InternalID:
            return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m_hwnd));
        case WindowProperty::Style:
            return static_cast<uint64_t>(m_style);
        case WindowProperty::ExStyle:
            return static_cast<uint64_t>(m_exStyle);
        case WindowProperty::ID:
            return static_cast<uint64_t>(m_windowId);
        case WindowProperty::ProcessID:
            return static_cast<uint64_t>(m_processId);
        case WindowProperty::ThreadID:
            return static_cast<uint64_t>(m_threadId);
        case WindowProperty::Title:
        case WindowProperty::Class:
        case WindowProperty::Size:
        case WindowProperty::Position:
        case WindowProperty::Process:
            return GetProperty(propertyId);
        default:
            return std::monostate{};
    }
}

std::string WindowInfo::GetProperty(int propertyId) const {
    switch (static_cast<WindowProperty>(propertyId)) {
        case WindowProperty::InternalID:
            return std::format("{:08X}", reinterpret_cast<uintptr_t>(m_hwnd)); // Legacy format often just 32-bit hex part, but we'll show what fits
        case WindowProperty::Title:
            return m_title;
        case WindowProperty::Class:
            return m_className;
        case WindowProperty::Size:
            return std::format("({}, {})", m_rect.right - m_rect.left, m_rect.bottom - m_rect.top);
        case WindowProperty::Position:
            return std::format("({}, {})", m_rect.top, m_rect.left); // Legacy: Top, Left
        case WindowProperty::Style:
            return std::format("0x{:08X}", m_style);
        case WindowProperty::ExStyle:
            return std::format("0x{:08X}", m_exStyle);
        case WindowProperty::ID:
            return std::to_string(m_windowId);
        case WindowProperty::ProcessID:
            return std::to_string(m_processId);
        case WindowProperty::ThreadID:
            return std::to_string(m_threadId);
        case WindowProperty::Process:
            return m_processName;
        default:
            return "";
    }
}

bool WindowInfo::MatchesFilter(const std::string& filter) const {
    if (filter.empty()) return true;

    // filter is pre-lowercased by caller
    if (utils::ToLower(m_title).find(filter) != std::string::npos) return true;
    if (utils::ToLower(m_className).find(filter) != std::string::npos) return true;
    if (utils::ToLower(m_processName).find(filter) != std::string::npos) return true;

    // Hex HWND match (using lowercase hex to match pre-lowercased filter)
    std::string hwndStr = std::format("{:x}", reinterpret_cast<uintptr_t>(m_hwnd));
    if (hwndStr.find(filter) != std::string::npos) return true;

    return false;
}

} // namespace pserv
