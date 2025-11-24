// pserv5.cpp : Defines the entry point for the application.
//

#include "precomp.h"
#include <config/settings.h>
#include <config/toml_backend.h>
#include <main_window.h>
#include <pserv5.h>
#include <utils/logging.h>
#include <utils/string_utils.h>
#include <crtdbg.h>
#include <core/data_controller_library.h>
#include <utils/base_app.h>

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    using namespace pserv;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    utils::BaseApp baseApp;

    // Initialize main window
    MainWindow mainWindow;
    mainWindow.SetConfigBackend(baseApp.m_pBackend);
    mainWindow.SetAppDataPath(baseApp.m_appDataPath);
    if (!mainWindow.Initialize(hInstance))
    {
        spdlog::error("Failed to initialize main window");
        return 1;
    }

    // Show window and start message loop
    // MainWindow will handle splash screen and async loading internally
    spdlog::info("Starting application");
    mainWindow.Show(nCmdShow);
    int result = mainWindow.MessageLoop();

    // Save configuration on exit
    config::theSettings.save(*baseApp.m_pBackend);
    spdlog::info("Configuration saved to: {}", baseApp.m_configPath.string());

    spdlog::info("pserv5 shutting down");
    return result;
}
