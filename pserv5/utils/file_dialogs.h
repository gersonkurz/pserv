#pragma once
#include <string>
#include <vector>
#include <Windows.h>

namespace pserv {
namespace utils {

/**
 * File type filter for save/open dialogs.
 */
struct FileTypeFilter {
    std::wstring name;      // Display name (e.g., L"JSON Files")
    std::wstring pattern;   // Pattern (e.g., L"*.json")
};

/**
 * Show a native Windows file save dialog using IFileSaveDialog COM interface.
 * @param hwnd Parent window handle
 * @param title Dialog title
 * @param defaultFileName Default file name (without extension)
 * @param filters File type filters
 * @param defaultFilterIndex Zero-based index of default filter
 * @param outFilePath [out] Selected file path (empty if cancelled)
 * @return true if user selected a file, false if cancelled
 */
bool SaveFileDialog(
    HWND hwnd,
    const std::wstring& title,
    const std::wstring& defaultFileName,
    const std::vector<FileTypeFilter>& filters,
    int defaultFilterIndex,
    std::wstring& outFilePath);

} // namespace utils
} // namespace pserv
