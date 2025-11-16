// pserv5.cpp : Defines the entry point for the application.
//

#include "precomp.h"
#include "framework.h"
#include "pserv5.h"
#include "utils/logging.h"
#include "utils/string_utils.h"
#include "Config/settings.h"
#include "Config/toml_backend.h"
#include "main_window.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Step 1: Initialize logging with default settings
    auto logger = pserv::utils::InitializeLogging();
    logger->info("pserv5 starting up");

    // Step 2: Load configuration
    std::filesystem::path configPath = "pserv5.toml";
    logger->info("Loading configuration from: {}", std::filesystem::absolute(configPath).string());

    pserv::config::TomlBackend backend{configPath.string()};
    pserv::config::theSettings.load(backend);

    // Step 3: Apply logging configuration from loaded settings
    std::string logLevel = pserv::config::theSettings.logging.logLevel.get();
    logger->set_level(spdlog::level::from_str(logLevel));
    logger->info("Log level set to: {}", logLevel);

    // Initialize main window
    pserv::MainWindow mainWindow;
    mainWindow.SetConfigBackend(&backend);
    if (!mainWindow.Initialize(hInstance)) {
        logger->error("Failed to initialize main window");
        return 1;
    }

    mainWindow.Show(nCmdShow);
    int result = mainWindow.MessageLoop();

    // Save configuration on exit
    pserv::config::theSettings.save(backend);
    logger->info("Configuration saved to: {}", std::filesystem::absolute(configPath).string());

    logger->info("pserv5 shutting down");
    return result;
}
