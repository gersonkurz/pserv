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


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    int flag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flag |= _CRTDBG_LEAK_CHECK_DF;
    flag |= _CRTDBG_ALLOC_MEM_DF;
    _CrtSetDbgFlag(flag);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    // Change this to leaking allocation's number to break there
    _CrtSetBreakAlloc(-1);


    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Step 1: Initialize logging with console/debug output only
    auto logger = pserv::utils::InitializeLogging();
    logger->info("pserv5 starting up");

    // Step 2: Get AppData path and determine config/log paths
    std::filesystem::path appDataPath = pserv::utils::GetAppDataPath();
    std::filesystem::path configPath = appDataPath / "pserv5.toml";
    logger->info("AppData path: {}", appDataPath.string());
    logger->info("Loading configuration from: {}", configPath.string());

    // Step 3: Load configuration
    pserv::config::TomlBackend backend{configPath.string()};
    pserv::config::theSettings.load(backend);
    logger->info("Config loaded - activeView value: '{}'", pserv::config::theSettings.application.activeView.get());

    // Step 4: Configure file logging
    std::string logFilePath = pserv::config::theSettings.logging.logFilePath.get();
    if (logFilePath.empty())
    {
        // Default: AppData/pserv5/pserv5.log
        logFilePath = (appDataPath / "pserv5.log").string();
        pserv::config::theSettings.logging.logFilePath.set(logFilePath);
    }
    logger->info("Log file path: {}", logFilePath);
    pserv::utils::ReconfigureLoggingWithFile(logFilePath);

    // Step 5: Apply log level from config
    std::string logLevel = pserv::config::theSettings.logging.logLevel.get();
    logger->set_level(spdlog::level::from_str(logLevel));
    logger->info("Log level set to: {}", logLevel);

    // Initialize main window
    pserv::MainWindow mainWindow;
    mainWindow.SetConfigBackend(&backend);
    if (!mainWindow.Initialize(hInstance))
    {
        logger->error("Failed to initialize main window");
        return 1;
    }

    // Show window and start message loop
    // MainWindow will handle splash screen and async loading internally
    logger->info("Starting application");
    mainWindow.Show(nCmdShow);
    int result = mainWindow.MessageLoop();

    // Save configuration on exit
    pserv::config::theSettings.save(backend);
    logger->info("Configuration saved to: {}", configPath.string());

    logger->info("pserv5 shutting down");
    return result;
}
