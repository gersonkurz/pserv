#include "precomp.h"
#include "logging.h"
#include <spdlog/sinks/msvc_sink.h>
#include <chrono>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace pserv::utils {

    void NdjsonFormatter::format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) {
        // Get timestamp in ISO8601 format
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm_buf;
        localtime_s(&tm_buf, &time_t_now);

        // Build timestamp string
        std::string timestamp = std::format(
            "{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}",
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, ms.count()
        );

        // Build NDJSON using RapidJSON (properly escapes all strings)
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        doc.AddMember("timestamp", rapidjson::Value(timestamp.c_str(), allocator), allocator);
        doc.AddMember("level", rapidjson::Value(spdlog::level::to_string_view(msg.level).data(), allocator), allocator);
        doc.AddMember("logger", rapidjson::Value(msg.logger_name.data(), static_cast<rapidjson::SizeType>(msg.logger_name.size()), allocator), allocator);
        doc.AddMember("message", rapidjson::Value(msg.payload.data(), static_cast<rapidjson::SizeType>(msg.payload.size()), allocator), allocator);

        // Only include source location if we have a valid filename
        if (msg.source.filename != nullptr) {
            std::string source = std::format("{}:{}", msg.source.filename, msg.source.line);
            doc.AddMember("source", rapidjson::Value(source.c_str(), allocator), allocator);
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        dest.append(buffer.GetString(), buffer.GetString() + buffer.GetSize());
        dest.append("\n", "\n" + 1);
    }

    std::unique_ptr<spdlog::formatter> NdjsonFormatter::clone() const {
        return std::make_unique<NdjsonFormatter>();
    }

    std::shared_ptr<spdlog::logger> InitializeLogging() {
        // TODO: Remove this debug code - delete log file on startup for clean logs during development
        std::remove("pserv5.log");

        // Console sink with default pattern
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);

        // MSVC OutputDebugString sink for Visual Studio debugger
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        msvc_sink->set_level(spdlog::level::debug);

        // File sink with NDJSON format for Splunk
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "pserv5.log", 1024 * 1024 * 10, 3);
        file_sink->set_level(spdlog::level::debug);
        file_sink->set_formatter(std::make_unique<NdjsonFormatter>());

        std::vector<spdlog::sink_ptr> sinks{console_sink, msvc_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("pserv5", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::debug);
        spdlog::set_default_logger(logger);

        return logger;
    }
}
