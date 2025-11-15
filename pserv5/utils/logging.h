#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>

namespace pserv::utils {
    std::shared_ptr<spdlog::logger> InitializeLogging();

    // Custom NDJSON formatter for Splunk ingestion
    class NdjsonFormatter : public spdlog::formatter {
    public:
        void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override;
        std::unique_ptr<spdlog::formatter> clone() const override;
    };
}
