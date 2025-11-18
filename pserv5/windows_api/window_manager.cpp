#include "precomp.h"
#include "window_manager.h"
#include <psapi.h>
#include "../utils/string_utils.h"

namespace pserv {

// Helper struct for EnumWindows callback
struct EnumContext {
    std::vector<WindowInfo*> windows;
};

std::vector<WindowInfo*> WindowManager::EnumerateWindows() {
    EnumContext context;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&context));
    return context.windows;
}

BOOL CALLBACK WindowManager::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumContext* context = reinterpret_cast<EnumContext*>(lParam);

    // Get basic info
    std::string title = GetWindowTextUtf8(hwnd);
    if (title.empty()) {
        // Skip windows with no title (mimicking legacy behavior/common practice to filter hidden message windows)
        return TRUE;
    }

    auto* info = new WindowInfo(hwnd);
    info->SetTitle(std::move(title));
    info->SetClassName(GetClassNameUtf8(hwnd));

    // Rect
    RECT r;
    if (GetWindowRect(hwnd, &r)) {
        info->SetRect(r);
    }

    // Style & ExStyle
    info->SetStyle(static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)));
    info->SetExStyle(static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));
    info->SetWindowId(static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWLP_ID)));

    // Process & Thread
    DWORD pid = 0;
    DWORD tid = GetWindowThreadProcessId(hwnd, &pid);
    info->SetProcessId(pid);
    info->SetThreadId(tid);
    info->SetProcessName(GetProcessName(pid));

    // State determination
    bool isDisabled = false;
    bool isRunning = true;

    int width = r.right - r.left;
    int height = r.bottom - r.top;

    // Legacy logic: Disabled if 0x0 size or not visible
    if (width == 0 && height == 0) {
        isDisabled = true;
    }
    if (!IsWindowVisible(hwnd)) {
        isDisabled = true;
    }

    // Additional check: Hung window?
    if (IsHungAppWindow(hwnd)) {
        isRunning = false; 
    }

    info->SetDisabled(isDisabled);
    info->SetRunning(isRunning);

    context->windows.push_back(info);
    return TRUE;
}

bool WindowManager::ShowWindow(HWND hwnd, int nCmdShow) {
    return ::ShowWindow(hwnd, nCmdShow) != 0;
}

bool WindowManager::CloseWindow(HWND hwnd) {
    // Send WM_CLOSE instead of just CloseWindow (which minimizes)
    return ::PostMessageW(hwnd, WM_CLOSE, 0, 0) != 0;
}

bool WindowManager::BringToFront(HWND hwnd) {
    if (::IsIconic(hwnd)) {
        ::ShowWindow(hwnd, SW_RESTORE);
    }
    return ::SetForegroundWindow(hwnd) != 0;
}

std::string WindowManager::GetWindowTextUtf8(HWND hwnd) {
    int len = ::GetWindowTextLengthW(hwnd);
    if (len <= 0) return "";

    std::vector<wchar_t> buf(len + 1);
    ::GetWindowTextW(hwnd, buf.data(), len + 1);
    return utils::WideToUtf8(buf.data());
}

std::string WindowManager::GetClassNameUtf8(HWND hwnd) {
    wchar_t buf[256];
    if (::GetClassNameW(hwnd, buf, 256) == 0) return "";
    return utils::WideToUtf8(buf);
}

std::string WindowManager::GetProcessName(DWORD pid) {
    std::string name;
    HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
        wchar_t buf[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, buf, &size)) {
            // Extract just filename
            wchar_t* filePart = wcsrchr(buf, L'\\');
            if (filePart) {
                name = utils::WideToUtf8(filePart + 1);
            } else {
                name = utils::WideToUtf8(buf);
            }
        }
        ::CloseHandle(hProcess);
    }
    return name;
}

} // namespace pserv
