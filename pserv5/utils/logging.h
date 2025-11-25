/// @file logging.h
/// @brief Application logging initialization and configuration.
///
/// Provides spdlog-based logging setup with console, debug output,
/// and file sinks. Supports NDJSON format for log aggregation.
#pragma once

namespace pserv
{
    namespace utils
    {
        /// @brief Get the application data directory path.
        /// Creates %APPDATA%/pserv5 if it doesn't exist.
        std::filesystem::path GetAppDataPath();

        /// @brief Initialize logging with console/debug output only.
        /// Call early in startup before config is loaded.
        std::shared_ptr<spdlog::logger> InitializeLogging();

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
