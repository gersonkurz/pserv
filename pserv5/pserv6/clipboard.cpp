#include "pch.h"
#include <utils/string_utils.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

namespace pserv::utils
{
    void CopyToClipboard(const std::string& text)
    {
        winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
        package.SetText(Utf8ToWide(text));
        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::Flush();
    }
}
