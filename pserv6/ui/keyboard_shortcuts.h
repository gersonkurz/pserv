#pragma once

namespace pserv::winui
{
    enum class Shortcut { None, Refresh, Search, ToggleRefresh, Provider, ZoomIn, ZoomOut, ResetZoom, DeleteSelection };
    struct ShortcutCommand
    {
        Shortcut command{Shortcut::None};
        int provider{-1};
    };

    // Windows virtual-key values; independent of XAML so routing can be tested
    // without creating a window or executing an administrative action.
    constexpr ShortcutCommand DecodeShortcut(int key, bool control, bool shift, bool alt,
        bool textInput, bool tableFocused, bool dialogOpen)
    {
        if (dialogOpen || alt) return {};
        if (!control && !shift && key == 0x74) return {Shortcut::Refresh}; // F5
        if (control && !shift && key == 'F') return {Shortcut::Search};
        if (control && !shift && key == 'R') return {Shortcut::ToggleRefresh};
        if (textInput) return {};
        if (control)
        {
            if (shift && key == '0') return {Shortcut::ResetZoom};
            if (!shift && key >= '1' && key <= '9') return {Shortcut::Provider, key - '1'};
            if (!shift && key == '0') return {Shortcut::Provider, 9};
            if (key == 0xBB || key == 0x6B) return {Shortcut::ZoomIn}; // OEM_PLUS / ADD
            if (!shift && (key == 0xBD || key == 0x6D)) return {Shortcut::ZoomOut};
        }
        if (!control && !shift && tableFocused && key == 0x2E) return {Shortcut::DeleteSelection};
        return {};
    }
}
