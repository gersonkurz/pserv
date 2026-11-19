#pragma once
#include "App.xaml.g.h"
#include <utils/base_app.h>

namespace winrt::pserv6::implementation
{
    struct App : AppT<App>
    {
        App();
        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const &);

    private:
        /// Logging + settings (plan D13: %APPDATA%\pserv6\config.toml, first run imports pserv5's).
        std::unique_ptr<pserv::utils::BaseApp> m_pBaseApp;
        winrt::Microsoft::UI::Xaml::Window m_window{nullptr};
    };
}
