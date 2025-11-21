#include "precomp.h"
#include <utils/file_dialogs.h>
#include <utils/string_utils.h>

namespace pserv {
namespace utils {

bool SaveFileDialog(
    HWND hwnd,
    const std::wstring& title,
    const std::wstring& defaultFileName,
    const std::vector<FileTypeFilter>& filters,
    int defaultFilterIndex,
    std::wstring& outFilePath)
{
    outFilePath.clear();

    // Initialize COM (safe to call multiple times)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool bComInitialized = SUCCEEDED(hr);

    // Create IFileSaveDialog instance
    wil::com_ptr<IFileSaveDialog> pFileSave;
    hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL,
        IID_PPV_ARGS(&pFileSave));

    if (FAILED(hr)) {
        spdlog::error("Failed to create FileSaveDialog: HRESULT {:#x}", static_cast<unsigned>(hr));
        if (bComInitialized) CoUninitialize();
        return false;
    }

    // Set title
    if (!title.empty()) {
        pFileSave->SetTitle(title.c_str());
    }

    // Set default file name
    if (!defaultFileName.empty()) {
        pFileSave->SetFileName(defaultFileName.c_str());
    }

    // Set file type filters
    if (!filters.empty()) {
        std::vector<COMDLG_FILTERSPEC> filterSpecs;
        filterSpecs.reserve(filters.size());

        for (const auto& filter : filters) {
            COMDLG_FILTERSPEC spec;
            spec.pszName = filter.name.c_str();
            spec.pszSpec = filter.pattern.c_str();
            filterSpecs.push_back(spec);
        }

        pFileSave->SetFileTypes(static_cast<UINT>(filterSpecs.size()), filterSpecs.data());

        // Set default filter index (1-based for COM)
        if (defaultFilterIndex >= 0 && defaultFilterIndex < static_cast<int>(filters.size())) {
            pFileSave->SetFileTypeIndex(defaultFilterIndex + 1);
        }
    }

    // Show the dialog
    hr = pFileSave->Show(hwnd);

    if (SUCCEEDED(hr)) {
        // Get the selected file path
        wil::com_ptr<IShellItem> pItem;
        hr = pFileSave->GetResult(&pItem);

        if (SUCCEEDED(hr)) {
            wil::unique_cotaskmem_string pszFilePath;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

            if (SUCCEEDED(hr)) {
                outFilePath = pszFilePath.get();
                spdlog::info("SaveFileDialog: Selected file '{}'",
                    WideToUtf8(outFilePath));
            }
        }
    } else if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        spdlog::debug("SaveFileDialog: User cancelled");
    } else {
        spdlog::error("SaveFileDialog failed: HRESULT {:#x}", static_cast<unsigned>(hr));
    }

    if (bComInitialized) {
        CoUninitialize();
    }

    return !outFilePath.empty();
}

} // namespace utils
} // namespace pserv
