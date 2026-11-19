/// @file window_manager.h
/// @brief Windows window enumeration and manipulation API wrapper.
///
/// Provides top-level window listing and control operations like
/// show, hide, close, and bring to front.
#pragma once

namespace pserv
{
    class DataObjectContainer;

    /// @brief Static class for window enumeration and manipulation.
    ///
    /// Uses EnumWindows API for enumeration and various Win32 window
    /// functions for control operations.
    class WindowManager final
    {
    public:
        /// @brief Enumerate all top-level windows into a container.
        /// @param doc Container to populate with WindowInfo objects.
        static void EnumerateWindows(DataObjectContainer* doc);

        /// @brief Change window visibility state.
        /// @param hwnd Window handle.
        /// @param nCmdShow SW_SHOW, SW_HIDE, SW_MINIMIZE, SW_MAXIMIZE, etc.
        static bool ShowWindow(HWND hwnd, int nCmdShow);

        /// @brief Send WM_CLOSE to a window.
        static bool CloseWindow(HWND hwnd);

        /// @brief Bring window to foreground and activate it.
        static bool BringToFront(HWND hwnd);

    private:
        static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
        static std::string GetWindowTextUtf8(HWND hwnd);
        static std::string GetClassNameUtf8(HWND hwnd);
        static std::string GetProcessName(DWORD pid);
    };

} // namespace pserv
