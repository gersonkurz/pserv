#include "precomp.h"
#include <utils/logging.h>
#include "Knownfolders.h"
#include <utils/string_utils.h>
#include <Shlobj.h>

namespace pserv::utils
{
#ifdef PSERV_CONSOLE_BUILD
    void CopyToClipboard(const std::string &utf8_text)
    {
        std::wstring text = Utf8ToWide(utf8_text);
        // Clipboard operations must be serialized by OpenClipboard/CloseClipboard.
        if (!OpenClipboard(nullptr))
            throw std::runtime_error("OpenClipboard failed");

        // Ensure CloseClipboard is called on all exits.
        struct ClipboardCloser
        {
            ~ClipboardCloser()
            {
                CloseClipboard();
            }
        } closer;

        if (!EmptyClipboard())
            throw std::runtime_error("EmptyClipboard failed");

        // Allocate a global memory object for the text.
        const size_t bytes = (text.size() + 1) * sizeof(wchar_t);

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!hMem)
            throw std::runtime_error("GlobalAlloc failed");

        // Lock the handle and copy the text to the buffer.
        void *ptr = GlobalLock(hMem);
        if (!ptr)
        {
            GlobalFree(hMem);
            throw std::runtime_error("GlobalLock failed");
        }

        memcpy(ptr, text.c_str(), bytes);
        GlobalUnlock(hMem);

        // Place the handle on the clipboard.
        // After SetClipboardData succeeds, the system owns hMem.
        if (!SetClipboardData(CF_UNICODETEXT, hMem))
        {
            GlobalFree(hMem); // only free if SetClipboardData failed
            throw std::runtime_error("SetClipboardData failed");
        }

        // Do NOT free hMem here; ownership transferred to clipboard.
    }
#endif

    std::filesystem::path GetAppDataPath()
    {
        // Get LOCALAPPDATA path
        PWSTR localAppDataPath = nullptr;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppDataPath);
        if (FAILED(hr))
        {
            return {};
        }

        const std::wstring wpath{localAppDataPath};
        CoTaskMemFree(localAppDataPath);

        // Convert to UTF-8 and append pserv5 folder
        const std::string appDataPath = utils::WideToUtf8(wpath);
        const std::filesystem::path path = std::filesystem::path(appDataPath) / "pserv5";

        // Create directory if it doesn't exist
        if (!std::filesystem::exists(path))
        {
            std::filesystem::create_directories(path);
        }

        return path;
    }

    void NdjsonFormatter::format(const spdlog::details::log_msg &msg, spdlog::memory_buf_t &dest)
    {
        // Get timestamp in ISO8601 format
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm_buf;
        localtime_s(&tm_buf, &time_t_now);

        // Build timestamp string
        std::string timestamp = std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}",
            tm_buf.tm_year + 1900,
            tm_buf.tm_mon + 1,
            tm_buf.tm_mday,
            tm_buf.tm_hour,
            tm_buf.tm_min,
            tm_buf.tm_sec,
            ms.count());

        // Build NDJSON using RapidJSON (properly escapes all strings)
        rapidjson::Document doc;
        doc.SetObject();
        auto &allocator = doc.GetAllocator();

        doc.AddMember("timestamp", rapidjson::Value(timestamp.c_str(), allocator), allocator);
        doc.AddMember("level", rapidjson::Value(spdlog::level::to_string_view(msg.level).data(), allocator), allocator);
        doc.AddMember("logger", rapidjson::Value(msg.logger_name.data(), static_cast<rapidjson::SizeType>(msg.logger_name.size()), allocator), allocator);
        doc.AddMember("message", rapidjson::Value(msg.payload.data(), static_cast<rapidjson::SizeType>(msg.payload.size()), allocator), allocator);

        // Only include source location if we have a valid filename
        if (msg.source.filename != nullptr)
        {
            std::string source = std::format("{}:{}", msg.source.filename, msg.source.line);
            doc.AddMember("source", rapidjson::Value(source.c_str(), allocator), allocator);
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        dest.append(buffer.GetString(), buffer.GetString() + buffer.GetSize());
        dest.append("\n", "\n" + 1);
    }

    std::unique_ptr<spdlog::formatter> NdjsonFormatter::clone() const
    {
        return std::make_unique<NdjsonFormatter>();
    }

    std::shared_ptr<spdlog::logger> InitializeLogging()
    {
        // Initialize with console and debug output only (before config is loaded)
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);

        // MSVC OutputDebugString sink for Visual Studio debugger
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        msvc_sink->set_level(spdlog::level::debug);

        std::vector<spdlog::sink_ptr> sinks{console_sink, msvc_sink};
        auto logger = std::make_shared<spdlog::logger>("pserv5", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::debug);
        spdlog::set_default_logger(logger);

        return logger;
    }

    void ReconfigureLoggingWithFile(const std::string& logFilePath)
    {
        // Get existing logger
        auto logger = spdlog::default_logger();

        // Force rotation on startup to create new log file
        std::filesystem::path log_path(logFilePath);
        if (std::filesystem::exists(log_path) && std::filesystem::file_size(log_path) > 0)
        {
            std::filesystem::path log_dir = log_path.parent_path();
            std::string log_stem = log_path.stem().string();
            std::string log_ext = log_path.extension().string();

            // Delete oldest log if it exists
            std::filesystem::path oldest = log_dir / (log_stem + ".10" + log_ext);
            if (std::filesystem::exists(oldest))
            {
                std::filesystem::remove(oldest);
            }

            // Shift all backup logs up (9 -> 10, 8 -> 9, etc.)
            for (int i = 9; i >= 1; --i)
            {
                std::filesystem::path old_path = log_dir / std::format("{}.{}{}", log_stem, i, log_ext);
                std::filesystem::path new_path = log_dir / std::format("{}.{}{}", log_stem, i + 1, log_ext);
                if (std::filesystem::exists(old_path))
                {
                    std::filesystem::rename(old_path, new_path);
                }
            }

            // Move current log to .1
            std::filesystem::rename(log_path, log_dir / (log_stem + ".1" + log_ext));
        }

        // Add file sink with NDJSON format - 10MB max size, keep 10 backups
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath, 1024 * 1024 * 10, 10);
        file_sink->set_level(spdlog::level::debug);
        file_sink->set_formatter(std::make_unique<NdjsonFormatter>());

        // Add file sink to existing logger
        logger->sinks().push_back(file_sink);
    }
} // namespace pserv::utils
