#include "precomp.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::pserv6::implementation
{
    App::App()
    {
        try
        {
            m_baseApp = std::make_unique<pserv::utils::BaseApp>();
        }
        catch (const std::exception &ex)
        {
            OutputDebugStringA(std::format("pserv6 BaseApp init failed: {}\n", ex.what()).c_str());
        }

        UnhandledException([](IInspectable const &, UnhandledExceptionEventArgs const &e)
        {
            spdlog::error("Unhandled XAML exception: 0x{:08X} - {}",
                static_cast<uint32_t>(e.Exception()),
                winrt::to_string(e.Message()));
            if (auto logger = spdlog::default_logger())
            {
                logger->flush();
            }
#if defined _DEBUG
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
#endif
        });
    }

    App::~App()
    {
        if (m_baseApp && m_baseApp->m_pBackend)
        {
            pserv::config::theSettings.save(*m_baseApp->m_pBackend);
            spdlog::info("Configuration saved to: {}", m_baseApp->m_configPath.string());
        }
        spdlog::info("pserv6 shutting down");
    }

    void App::OnLaunched(LaunchActivatedEventArgs const &)
    {
        try
        {
            spdlog::info("App::OnLaunched: Creating MainWindow...");
            auto mainWin = make_self<MainWindow>();
            if (m_baseApp)
            {
                mainWin->SetConfigBackend(m_baseApp->m_pBackend);
            }
            m_window = *mainWin;
            spdlog::info("App::OnLaunched: Activating MainWindow...");
            m_window.Activate();
            mainWin->BringToFront();
            spdlog::info("App::OnLaunched: MainWindow activated successfully");
        }
        catch (const winrt::hresult_error &ex)
        {
            spdlog::error("App::OnLaunched hresult_error: 0x{:08X} - {}", static_cast<uint32_t>(ex.code()), winrt::to_string(ex.message()));
        }
        catch (const std::exception &ex)
        {
            spdlog::error("App::OnLaunched std::exception: {}", ex.what());
        }
    }
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        ::winrt::Microsoft::UI::Xaml::Application::Start(
            [](auto &&)
            {
                ::winrt::make<::winrt::pserv6::implementation::App>();
            });
        return 0;
    }
    catch (const winrt::hresult_error &ex)
    {
        std::wstring msg = std::format(L"pserv6 startup failed: HRESULT 0x{:08X}\n{}",
            static_cast<uint32_t>(ex.code()),
            std::wstring_view(ex.message()));
        OutputDebugStringW((msg + L"\n").c_str());
        if (auto logger = spdlog::default_logger())
        {
            logger->error("{}", winrt::to_string(winrt::hstring(msg)));
            logger->flush();
        }
        fwprintf(stderr, L"%ls\n", msg.c_str());
        return static_cast<int>(ex.code());
    }
    catch (const std::exception &ex)
    {
        std::string msg = std::format("pserv6 startup failed: {}", ex.what());
        OutputDebugStringA((msg + "\n").c_str());
        if (auto logger = spdlog::default_logger())
        {
            logger->error("{}", msg);
            logger->flush();
        }
        fprintf(stderr, "%s\n", msg.c_str());
        return -1;
    }
}
