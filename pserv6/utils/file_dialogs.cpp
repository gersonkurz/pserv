#include "precomp.h"
#include <utils/file_dialogs.h>
#include <utils/string_utils.h>
#include <utils/win32_error.h>

namespace pserv
{
    namespace utils
    {

        bool SaveFileDialog(HWND hwnd,
            const std::wstring &title,
            const std::wstring &defaultFileName,
            const std::vector<FileTypeFilter> &filters,
            int defaultFilterIndex,
            std::wstring &outFilePath)
        {
            outFilePath.clear();

            // Initialize COM (safe to call multiple times)
            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            bool bComInitialized = SUCCEEDED(hr);

            if (!bComInitialized && hr != RPC_E_CHANGED_MODE)
            {
                // Log error unless it's already initialized with different threading model (expected in some cases)
                LogWin32ErrorCode("CoInitializeEx", hr);
            }

            // Create IFileSaveDialog instance
            wil::com_ptr<IFileSaveDialog> pFileSave;
            hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pFileSave));

            if (FAILED(hr))
            {
                LogWin32ErrorCode("CoCreateInstance(FileSaveDialog)", hr);
                if (bComInitialized)
                    CoUninitialize();
                return false;
            }

            // Set title
            if (!title.empty())
            {
                hr = pFileSave->SetTitle(title.c_str());
                if (FAILED(hr))
                {
                    LogWin32ErrorCode("IFileSaveDialog::SetTitle", hr);
                }
            }

            // Set default file name
            if (!defaultFileName.empty())
            {
                hr = pFileSave->SetFileName(defaultFileName.c_str());
                if (FAILED(hr))
                {
                    LogWin32ErrorCode("IFileSaveDialog::SetFileName", hr);
                }
            }

            // Set file type filters
            if (!filters.empty())
            {
                std::vector<COMDLG_FILTERSPEC> filterSpecs;
                filterSpecs.reserve(filters.size());

                for (const auto &filter : filters)
                {
                    COMDLG_FILTERSPEC spec;
                    spec.pszName = filter.name.c_str();
                    spec.pszSpec = filter.pattern.c_str();
                    filterSpecs.push_back(spec);
                }

                hr = pFileSave->SetFileTypes(static_cast<UINT>(filterSpecs.size()), filterSpecs.data());
                if (FAILED(hr))
                {
                    LogWin32ErrorCode("IFileSaveDialog::SetFileTypes", hr);
                }

                // Set default filter index (1-based for COM)
                if (defaultFilterIndex >= 0 && defaultFilterIndex < static_cast<int>(filters.size()))
                {
                    hr = pFileSave->SetFileTypeIndex(defaultFilterIndex + 1);
                    if (FAILED(hr))
                    {
                        LogWin32ErrorCode("IFileSaveDialog::SetFileTypeIndex", hr);
                    }
                }
            }

            // Show the dialog
            hr = pFileSave->Show(hwnd);

            if (SUCCEEDED(hr))
            {
                // Get the selected file path
                wil::com_ptr<IShellItem> pItem;
                hr = pFileSave->GetResult(&pItem);

                if (FAILED(hr))
                {
                    LogWin32ErrorCode("IFileSaveDialog::GetResult", hr);
                }
                else
                {
                    wil::unique_cotaskmem_string pszFilePath;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                    if (FAILED(hr))
                    {
                        LogWin32ErrorCode("IShellItem::GetDisplayName", hr);
                    }
                    else
                    {
                        outFilePath = pszFilePath.get();
                        spdlog::info("SaveFileDialog: Selected file '{}'", WideToUtf8(outFilePath));
                    }
                }
            }
            else if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            {
                spdlog::debug("SaveFileDialog: User cancelled");
            }
            else
            {
                LogWin32ErrorCode("IFileSaveDialog::Show", hr);
            }

            if (bComInitialized)
            {
                CoUninitialize();
            }

            return !outFilePath.empty();
        }

    } // namespace utils
} // namespace pserv
