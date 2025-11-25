/// @file base_app.h
/// @brief Application initialization and lifecycle management.
///
/// BaseApp handles startup sequence: memory leak detection, logging
/// initialization, config loading, and log file configuration.
#pragma once

#include <filesystem>
#include <crtdbg.h>
#include <config/toml_backend.h>
#include <utils/logging.h>
#include <spdlog/spdlog.h>
#include <config/settings.h>

namespace pserv
{
    namespace utils
    {
        /// @brief Application bootstrap class.
        ///
        /// Instantiate at the start of main() to initialize:
        /// 1. CRT memory leak detection (debug builds)
        /// 2. Logging with console/debug output
        /// 3. Configuration from TOML file
        /// 4. File logging with configured path
        /// 5. Log level from configuration
        class BaseApp final
        {
        public:
            BaseApp()
            {
                // setup memory leak detection
                int flag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
                flag |= _CRTDBG_LEAK_CHECK_DF;
                flag |= _CRTDBG_ALLOC_MEM_DF;
                _CrtSetDbgFlag(flag);
                _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
                _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
                // Change this to leaking allocation's number to break there
                _CrtSetBreakAlloc(-1);

                // Step 1: Initialize logging with console/debug output only
                auto logger = InitializeLogging();
                logger->info("pserv5 starting up");

                // Step 2: Get AppData path and determine config/log paths
                m_appDataPath = utils::GetAppDataPath();
                m_configPath = m_appDataPath / "pserv5.toml";
                logger->info("AppData path: {}", m_appDataPath.string());
                logger->info("Loading configuration from: {}", m_configPath.string());

                // Step 3: Load configuration
                m_pBackend = new config::TomlBackend{m_configPath.string()};
                config::theSettings.load(*m_pBackend);
                logger->info("Config loaded - activeView value: '{}'", config::theSettings.application.activeView.get());

                // Step 4: Configure file logging
                std::string logFilePath = config::theSettings.logging.logFilePath.get();
                if (logFilePath.empty())
                {
                    // Default: AppData/pserv5/pserv5.log
                    logFilePath = (m_appDataPath / "pserv5.log").string();
                    config::theSettings.logging.logFilePath.set(logFilePath);
                }
                logger->info("Log file path: {}", logFilePath);
                utils::ReconfigureLoggingWithFile(logFilePath);

                // Step 5: Apply log level from config
                std::string logLevel = config::theSettings.logging.logLevel.get();
                logger->set_level(spdlog::level::from_str(logLevel));
                logger->info("Log level set to: {}", logLevel);
            }

            ~BaseApp()
            {
                delete m_pBackend;
            }

            config::TomlBackend *m_pBackend = nullptr; ///< Configuration backend.
            std::filesystem::path m_appDataPath;        ///< %APPDATA%/pserv5 path.
            std::filesystem::path m_configPath;         ///< Path to pserv5.toml.
        };
    } // namespace utils
} // namespace pserv

