#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <shlobj.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::pserv6::implementation
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
        try
        {
            wil::unique_cotaskmem_string appData;
            check_hresult(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, appData.put()));
            auto logDirectory = std::filesystem::path(appData.get()) / L"pserv6";
            std::filesystem::create_directories(logDirectory);
            spdlog::set_default_logger(spdlog::rotating_logger_mt("pserv6", (logDirectory / L"winui.log").string(), 2 * 1024 * 1024, 3));
            spdlog::flush_on(spdlog::level::warn);
        }
        catch (...)
        {
            // A logging failure must not prevent the administration UI from opening.
            spdlog::set_default_logger(std::make_shared<spdlog::logger>("pserv6-debug", std::make_shared<spdlog::sinks::msvc_sink_mt>()));
        }
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            spdlog::error("Unhandled WinUI exception 0x{:08X}: {}", static_cast<uint32_t>(e.Exception().value), to_string(e.Message()));
        });
        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    /// <summary>
    /// Invoked when the application is launched.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        window = make<MainWindow>();
        window.Activate();
    }
}
