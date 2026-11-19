#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::pserv6::implementation
{
    App::App()
    {
#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const &, UnhandledExceptionEventArgs const &e)
            {
                if (IsDebuggerPresent())
                {
                    auto errorMessage = e.Message();
                    __debugbreak();
                }
            });
#endif
    }

    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const &e)
    {
        m_pBaseApp = std::make_unique<pserv::utils::BaseApp>("pserv6");

        m_window = make<MainWindow>();
        // Registered after MainWindow's own Closed handler, so the window has written its state
        m_window.Closed([this](auto &&, auto &&)
            {
                // A self-test changes layouts and sorts while it measures: never persist those
                if (!get_self<MainWindow>(m_window.as<winrt::pserv6::MainWindow>())->IsSelfTesting())
                {
                    pserv::config::theSettings.save(*m_pBaseApp->m_pBackend);
                    spdlog::info("Configuration saved to: {}", m_pBaseApp->m_configPath.string());
                }
                // The process may end without destroying the App (and with it BaseApp)
                pserv::utils::BaseApp::Flush();
            });
        m_window.Activate();
    }
}
