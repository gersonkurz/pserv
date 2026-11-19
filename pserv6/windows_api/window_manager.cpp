#include "precomp.h"
#include <utils/string_utils.h>
#include <utils/win32_error.h>
#include <windows_api/window_manager.h>
#include <models/window_info.h>
#include <core/data_object_container.h>

namespace pserv
{

    // Helper struct for EnumWindows callback
    struct EnumContext
    {
        DataObjectContainer* doc;
    };

    void WindowManager::EnumerateWindows(DataObjectContainer* doc)
    {
        EnumContext context{doc};
        if (!EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&context)))
        {
            LogWin32Error("EnumWindows");
        }
    }

    BOOL CALLBACK WindowManager::EnumWindowsProc(HWND hwnd, LPARAM lParam)
    {
        EnumContext *context = reinterpret_cast<EnumContext *>(lParam);

        // Get basic info
        auto title = GetWindowTextUtf8(hwnd);
        if (title.empty())
        {
            // Skip windows with no title (mimicking legacy behavior/common practice to filter hidden message windows)
            return TRUE;
        }
        
        const auto stableId{WindowInfo::GetStableID(hwnd)};
        auto info = context->doc->GetByStableId<WindowInfo>(stableId);
        if (info == nullptr)
        {
            info = context->doc->Append<WindowInfo>(DBG_NEW WindowInfo{hwnd});
        }

        info->SetTitle(std::move(title));
        info->SetClassName(GetClassNameUtf8(hwnd));

        // Rect
        RECT r;
        if (!GetWindowRect(hwnd, &r))
        {
            LogExpectedWin32Error("GetWindowRect", "HWND {:#x}", reinterpret_cast<uintptr_t>(hwnd));
        }
        else
        {
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
        if (width == 0 && height == 0)
        {
            isDisabled = true;
        }
        if (!IsWindowVisible(hwnd))
        {
            isDisabled = true;
        }

        // Additional check: Hung window?
        if (IsHungAppWindow(hwnd))
        {
            isRunning = false;
        }

        info->SetDisabled(isDisabled);
        info->SetRunning(isRunning);
        return TRUE;
    }

    bool WindowManager::ShowWindow(HWND hwnd, int nCmdShow)
    {
        BOOL result = ::ShowWindow(hwnd, nCmdShow);
        // ShowWindow returns previous visibility state, not error status
        // No error logging needed - return value indicates state, not success
        return result != 0;
    }

    bool WindowManager::CloseWindow(HWND hwnd)
    {
        // Send WM_CLOSE instead of just CloseWindow (which minimizes)
        if (!::PostMessageW(hwnd, WM_CLOSE, 0, 0))
        {
            LogWin32Error("PostMessageW(WM_CLOSE)", "HWND {:#x}", reinterpret_cast<uintptr_t>(hwnd));
            return false;
        }
        return true;
    }

    bool WindowManager::BringToFront(HWND hwnd)
    {
        if (::IsIconic(hwnd))
        {
            ::ShowWindow(hwnd, SW_RESTORE);
        }

        if (!::SetForegroundWindow(hwnd))
        {
            LogWin32Error("SetForegroundWindow", "HWND {:#x}", reinterpret_cast<uintptr_t>(hwnd));
            return false;
        }
        return true;
    }

    std::string WindowManager::GetWindowTextUtf8(HWND hwnd)
    {
        int len = ::GetWindowTextLengthW(hwnd);
        if (len <= 0)
        {
            // Zero length is valid (empty title), don't log
            return "";
        }

        std::vector<wchar_t> buf(len + 1);
        int result = ::GetWindowTextW(hwnd, buf.data(), len + 1);
        if (result == 0 && len > 0)
        {
            LogExpectedWin32Error("GetWindowTextW", "HWND {:#x}", reinterpret_cast<uintptr_t>(hwnd));
            return "";
        }
        return utils::WideToUtf8(buf.data());
    }

    std::string WindowManager::GetClassNameUtf8(HWND hwnd)
    {
        wchar_t buf[256];
        if (::GetClassNameW(hwnd, buf, 256) == 0)
        {
            LogExpectedWin32Error("GetClassNameW", "HWND {:#x}", reinterpret_cast<uintptr_t>(hwnd));
            return "";
        }
        return utils::WideToUtf8(buf);
    }

    std::string WindowManager::GetProcessName(DWORD pid)
    {
        std::string name;
        HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess)
        {
            LogExpectedWin32Error("OpenProcess", "PID {} for window process name", pid);
            return name;
        }

        wchar_t buf[MAX_PATH];
        DWORD size = MAX_PATH;
        if (!QueryFullProcessImageNameW(hProcess, 0, buf, &size))
        {
            LogExpectedWin32Error("QueryFullProcessImageNameW", "PID {}", pid);
        }
        else
        {
            // Extract just filename
            wchar_t *filePart = wcsrchr(buf, L'\\');
            if (filePart)
            {
                name = utils::WideToUtf8(filePart + 1);
            }
            else
            {
                name = utils::WideToUtf8(buf);
            }
        }
        ::CloseHandle(hProcess);
        return name;
    }

} // namespace pserv
