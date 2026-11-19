#pragma once
#include "App.xaml.g.h"
#include <utils/base_app.h>

namespace winrt::pserv5::implementation
{
    struct App : AppT<App>
    {
        App();
        ~App();

        void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const &args);

    private:
        std::unique_ptr<pserv::utils::BaseApp> m_baseApp;
        winrt::Microsoft::UI::Xaml::Window m_window{nullptr};
    };
}
