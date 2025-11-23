#pragma once

namespace pserv
{    
    namespace utils
    {        
        // Get application data directory path (creates if doesn't exist)
        std::filesystem::path GetAppDataPath();

        // Initialize logging with console/debug output only (before config loaded)
        std::shared_ptr<spdlog::logger> InitializeLogging();

        // Reconfigure logging with file sink after config is loaded
        void ReconfigureLoggingWithFile(const std::string &logFilePath);

        // Custom NDJSON formatter for Splunk ingestion
        class NdjsonFormatter : public spdlog::formatter
        {
        public:
            void format(const spdlog::details::log_msg &msg, spdlog::memory_buf_t &dest) override;
            std::unique_ptr<spdlog::formatter> clone() const override;
        };
    } // namespace utils
} // namespace pserv::utils
