#pragma once

namespace pserv
{
    class DataObject;

    class WindowManager
    {
    public:
        // Enumerates all top-level windows
        static std::vector<DataObject *> EnumerateWindows();

        // Window actions
        static bool ShowWindow(HWND hwnd, int nCmdShow);
        static bool CloseWindow(HWND hwnd);
        static bool BringToFront(HWND hwnd);

    private:
        static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
        static std::string GetWindowTextUtf8(HWND hwnd);
        static std::string GetClassNameUtf8(HWND hwnd);
        static std::string GetProcessName(DWORD pid);
    };

} // namespace pserv
