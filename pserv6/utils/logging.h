/// @file logging.h
/// @brief Application logging initialization and configuration.
///
/// Provides spdlog-based logging setup with console, debug output,
/// and file sinks. Supports NDJSON format for log aggregation.
#pragma once
#include <ShlObj.h> // KNOWNFOLDERID, FOLDERID_*

namespace pserv
{
    namespace utils
    {
        /// @brief Get (and create if missing) the folder @p appName below the known folder
        /// @p folder (pserv6: %APPDATA%\pserv6; pserv5's, for the first-run import: %LOCALAPPDATA%\pserv5).
        std::filesystem::path GetAppDataPath(REFKNOWNFOLDERID folder, std::string_view appName);

        /// @brief Initialize logging with console/debug output only.
        /// Call early in startup before config is loaded.
        /// @param name Logger name (the program), written into every log line.
        std::shared_ptr<spdlog::logger> InitializeLogging(std::string_view name);

        /// @brief Add file sink to logging after config is loaded.
        /// @param logFilePath Path to log file.
        void ReconfigureLoggingWithFile(const std::string &logFilePath);

        /// @brief Custom NDJSON formatter for log aggregation (e.g., Splunk).
        class NdjsonFormatter : public spdlog::formatter
        {
        public:
            void format(const spdlog::details::log_msg &msg, spdlog::memory_buf_t &dest) override;
            std::unique_ptr<spdlog::formatter> clone() const override;
        };
    } // namespace utils
} // namespace pserv
