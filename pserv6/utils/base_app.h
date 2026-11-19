/// @file base_app.h
/// @brief Application initialization and lifecycle management.
///
/// BaseApp handles the startup sequence shared by pserv6 and pservc: memory leak detection,
/// logging, configuration loading (with the first-run import from pserv5) and the log file.
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
        /// @brief Where the configuration lives: one file shared by pserv6 and pservc,
        /// %APPDATA%\pserv6\config.toml (plan D13). pservc only reads it.
        inline std::filesystem::path GetConfigFolder()
        {
            return GetAppDataPath(FOLDERID_RoamingAppData, "pserv6");
        }

        /// @brief Application bootstrap. Instantiate at the start of the program:
        /// 1. CRT memory leak detection (debug builds)
        /// 2. Logging to the debugger
        /// 3. Configuration from %APPDATA%\pserv6\config.toml; on the first run (no such file yet),
        ///    pserv5's %LOCALAPPDATA%\pserv5\pserv5.toml is loaded instead (D13), without the font
        ///    size and theme (D11: pserv6 follows the Fluent theme)
        /// 4. File logging to <config folder>\<logFileName>, one file per program
        /// 5. Log level from configuration
        class BaseApp final
        {
        public:
            /// @param logName Program name for log lines and the log file (<logName>.log).
            explicit BaseApp(std::string_view logName)
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

                // Step 1: Initialize logging with debugger output only
                auto logger = InitializeLogging(logName);
                logger->info("{} starting up", logName);

                // Step 2: Config folder and file
                m_appDataPath = GetConfigFolder();
                m_configPath = m_appDataPath / "config.toml";
                logger->info("Configuration folder: {}", m_appDataPath.string());

                // Step 3: Load configuration (on the first run, pserv5's)
                std::error_code ec;
                const auto pserv5Config = GetAppDataPath(FOLDERID_LocalAppData, "pserv5") / "pserv5.toml";
                if (!std::filesystem::exists(m_configPath, ec) && std::filesystem::exists(pserv5Config, ec))
                {
                    logger->info("First run: importing configuration from {}", pserv5Config.string());
                    config::TomlBackend importBackend{pserv5Config.string()};
                    config::theSettings.load(importBackend);
                    config::theSettings.application.fontSizeScaled.revertToDefault();
                    config::theSettings.application.theme.revertToDefault();
                    m_bImported = true;
                }
                m_pBackend = new config::TomlBackend{m_configPath.string()};
                if (!m_bImported)
                {
                    config::theSettings.load(*m_pBackend);
                }
                logger->info("Config loaded from {} - activeView value: '{}'", m_bImported ? pserv5Config.string() : m_configPath.string(),
                    config::theSettings.application.activeView.get());

                // Step 4: File logging, one file per program next to the configuration
                const auto logFilePath = (m_appDataPath / std::format("{}.log", logName)).string();
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
                Flush();
            }

            BaseApp(const BaseApp &) = delete;
            BaseApp &operator=(const BaseApp &) = delete;

            /// @brief Write out buffered log lines (also call it where a frontend's exit path
            /// may not run this destructor).
            static void Flush()
            {
                if (auto logger = spdlog::default_logger())
                    logger->flush();
            }

            config::TomlBackend *m_pBackend = nullptr; ///< Configuration backend.
            std::filesystem::path m_appDataPath;        ///< %APPDATA%\pserv6.
            std::filesystem::path m_configPath;         ///< %APPDATA%\pserv6\config.toml.
            bool m_bImported{false};                    ///< Settings came from pserv5's file (first run).
        };
    } // namespace utils
} // namespace pserv
