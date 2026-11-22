#pragma once
#include <core/data_controller.h>

namespace pserv6
{
    /// Segoe Fluent Icons glyph for a view (navigation pane).
    inline const wchar_t *ViewGlyph(std::string_view name)
    {
        static const std::pair<std::string_view, const wchar_t *> glyphs[] = {
            {pserv::SERVICES_DATA_CONTROLLER_NAME, L"\uE713"},            // Settings
            {pserv::DEVICES_DATA_CONTROLLER_NAME, L"\uE772"},             // Devices
            {pserv::PROCESSES_DATA_CONTROLLER_NAME, L"\uE9D9"},           // Diagnostic
            {pserv::WINDOWS_DATA_CONTROLLER_NAME, L"\uE78B"},             // NewWindow
            {pserv::MODULES_DATA_CONTROLLER_NAME, L"\uE8F1"},             // Library
            {pserv::UNINSTALLER_DATA_CONTROLLER_NAME, L"\uE71D"},         // AllApps
            {pserv::ENVIRONMENT_VARIABLES_CONTROLLER_NAME, L"\uE943"},    // Code
            {pserv::STARTUP_PROGRAMS_DATA_CONTROLLER_NAME, L"\uE7E8"},    // PowerButton
            {pserv::NETWORK_CONNECTIONS_DATA_CONTROLLER_NAME, L"\uE968"}, // Network
            {pserv::SCHEDULED_TASKS_DATA_CONTROLLER_NAME, L"\uE823"},     // Recent (clock)
        };
        for (const auto &[view, glyph] : glyphs)
            if (view == name)
                return glyph;
        return L"\uE8A5"; // Document
    }

    /// For an action without a glyph where a button needs one (the name is its tooltip).
    inline constexpr const wchar_t *MoreGlyph = L"\uE712";

    /// Segoe Fluent Icons glyph for an action, by its name; nullptr if it has none (no icon is
    /// better than a wrong one).
    inline const wchar_t *ActionGlyph(std::string_view name)
    {
        static const std::pair<std::string_view, const wchar_t *> exact[] = {
            {"Start", L"\uE768"},                   // Play
            {"Run Now", L"\uE768"},
            {"Stop", L"\uE71A"},                    // Stop
            {"Restart", L"\uE72C"},                 // Refresh
            {"Pause", L"\uE769"},                   // Pause
            {"Resume", L"\uE893"},                  // Next
            {"Enable", L"\uE73E"},                  // CheckMark
            {"Disable", L"\uF140"},                 // StatusCircleBlock
            {"Delete", L"\uE74D"},                  // Delete
            {"Uninstall", L"\uE74D"},
            {"Uninstall Service", L"\uE74D"},
            {"Delete Registry Key", L"\uE894"},  // Clear
            {"Terminate Process", L"\uE711"},       // Cancel
            {"Close", L"\uE711"},
            {"Close Connection", L"\uE711"},
            {"Open in Registry Editor", L"\uE90F"}, // Repair
            {"Open in Explorer", L"\uE838"},        // FolderOpen
            {"Open File Location", L"\uE838"},
            {"Open Containing Folder", L"\uE838"},
            {"Open Terminal Here", L"\uE756"},      // CommandPrompt
            {"Edit Configuration", L"\uE70F"},      // Edit
            {"Show", L"\uE890"},                    // View
            {"Hide", L"\uED1A"},                    // Hide
            {"Minimize", L"\uE921"},                // ChromeMinimize
            {"Maximize", L"\uE922"},                // ChromeMaximize
            {"Restore", L"\uE923"},                 // ChromeRestore
            {"Bring To Front", L"\uE8A7"},          // OpenInNewWindow
            {"Properties", L"\uE946"},
            {"Set Startup: Automatic", L"\uE895"},  // Sync
            {"Set Startup: Manual", L"\uE77B"},     // Contact (by hand)
            {"Set Startup: Disabled", L"\uE72E"},   // Lock
        };
        for (const auto &[action, glyph] : exact)
            if (action == name)
                return glyph;
        static const std::pair<std::string_view, const wchar_t *> prefixes[] = {
            {"Copy", L"\uE8C8"},        // Copy
            {"Add", L"\uE710"},         // Add
            {"Export", L"\uE74E"},      // Save
        };
        for (const auto &[prefix, glyph] : prefixes)
            if (name.starts_with(prefix))
                return glyph;
        return nullptr;
    }
} // namespace pserv6
