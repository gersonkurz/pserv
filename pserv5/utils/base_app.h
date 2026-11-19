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
        /// Where an application keeps its configuration and log.
        struct AppIdentity
        {
            std::string_view name{"pserv5"};          ///< Folder name and log tag.
            KNOWNFOLDERID folder{FOLDERID_LocalAppData};
            std::string_view configFileName{"pserv5.toml"};
            std::string_view logFileName{"pserv5.log"};
            /// Optional first-run import: when the config file does not exist yet and this one does,
            /// it is loaded instead (plan D13). Values that must not carry over are reset by the caller.
            std::filesystem::path importConfigFrom{};
        };

        class BaseApp final
        {
        public:
            explicit BaseApp(const AppIdentity &identity = {})
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
                logger->info("{} starting up", identity.name);

                // Step 2: Get AppData path and determine config/log paths
                m_appDataPath = utils::GetAppDataPath(identity.folder, identity.name);
                m_configPath = m_appDataPath / identity.configFileName;
                logger->info("AppData path: {}", m_appDataPath.string());

                // Step 3: Load configuration (on first run, from the import source if there is one)
                std::error_code ec;
                if (!identity.importConfigFrom.empty() && !std::filesystem::exists(m_configPath, ec) &&
                    std::filesystem::exists(identity.importConfigFrom, ec))
                {
                    logger->info("First run: importing configuration from {}", identity.importConfigFrom.string());
                    config::TomlBackend importBackend{identity.importConfigFrom.string()};
                    config::theSettings.load(importBackend);
                    m_bImported = true;
                }
                logger->info("Loading configuration from: {}", m_configPath.string());
                m_pBackend = new config::TomlBackend{m_configPath.string()};
                if (!m_bImported)
                {
                    config::theSettings.load(*m_pBackend);
                }
                logger->info("Config loaded - activeView value: '{}'", config::theSettings.application.activeView.get());

                // Step 4: Configure file logging
                std::string logFilePath = config::theSettings.logging.logFilePath.get();
                if (logFilePath.empty() || m_bImported)
                {
                    // Default: <AppData>/<name>/<logFileName>; an imported path is the other app's log
                    logFilePath = (m_appDataPath / identity.logFileName).string();
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
            std::filesystem::path m_appDataPath;        ///< The application's data folder.
            std::filesystem::path m_configPath;         ///< Its configuration file.
            bool m_bImported{false};                    ///< Settings came from AppIdentity::importConfigFrom.
        };
    } // namespace utils
} // namespace pserv

