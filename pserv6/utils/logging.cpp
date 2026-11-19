#include "precomp.h"
#include <utils/logging.h>
#include "Knownfolders.h"
#include <utils/string_utils.h>
#include <Shlobj.h>

namespace pserv::utils
{   
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

    std::filesystem::path GetAppDataPath(REFKNOWNFOLDERID folder, std::string_view appName)
    {
        PWSTR knownFolderPath = nullptr;
        HRESULT hr = SHGetKnownFolderPath(folder, 0, nullptr, &knownFolderPath);
        if (FAILED(hr))
        {
            return {};
        }

        const std::wstring wpath{knownFolderPath};
        CoTaskMemFree(knownFolderPath);

        const std::filesystem::path path = std::filesystem::path(wpath) / Utf8ToWide(std::string{appName});

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

    std::shared_ptr<spdlog::logger> InitializeLogging(std::string_view name)
    {
        // MSVC OutputDebugString sink for Visual Studio debugger only
        // NO console sink - console output should be done via console.h in pservc
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
        msvc_sink->set_level(spdlog::level::warn);

        std::vector<spdlog::sink_ptr> sinks{msvc_sink};
        auto logger = std::make_shared<spdlog::logger>(std::string{name}, sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::debug);

        // Explicitly drop the default logger to prevent any console output
        spdlog::drop_all();
        spdlog::set_default_logger(logger);

        return logger;
    }

    namespace
    {
        // True if another process has the file open. An exclusive open (no sharing) then fails with
        // ERROR_SHARING_VIOLATION. A missing file, or any other error, is "not held".
        bool IsHeldOpenByAnotherProcess(const std::filesystem::path &path)
        {
            wil::unique_hfile file{CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
            return !file && GetLastError() == ERROR_SHARING_VIOLATION;
        }

        // Shift pserv6.log -> pserv6.1.log -> ... -> pserv6.10.log, using only the std::error_code
        // overloads. Returns false, after logging a warning, at the first failure.
        bool RotateLogFiles(const std::filesystem::path &logPath, spdlog::logger &logger)
        {
            auto fail = [&](std::string_view what, const std::filesystem::path &p, const std::error_code &ec)
            {
                logger.warn("Log rotation failed: cannot {} '{}': {}", what, p.string(), ec.message());
                return false;
            };

            std::error_code ec;
            const bool bExists = std::filesystem::exists(logPath, ec);
            if (ec)
                return fail("check", logPath, ec);
            if (!bExists)
                return true;
            const auto size = std::filesystem::file_size(logPath, ec);
            if (ec)
                return fail("size", logPath, ec);
            if (size == 0)
                return true;

            const std::filesystem::path logDir = logPath.parent_path();
            const std::string stem = logPath.stem().string();
            const std::string ext = logPath.extension().string();
            auto numbered = [&](int i) { return logDir / std::format("{}.{}{}", stem, i, ext); };

            std::filesystem::remove(numbered(10), ec); // a missing file is not an error: returns false, clears ec
            if (ec)
                return fail("remove", numbered(10), ec);
            for (int i = 9; i >= 1; --i)
            {
                const bool bBackupExists = std::filesystem::exists(numbered(i), ec);
                if (ec)
                    return fail("check", numbered(i), ec);
                if (bBackupExists)
                {
                    std::filesystem::rename(numbered(i), numbered(i + 1), ec);
                    if (ec)
                        return fail("rename", numbered(i), ec);
                }
            }
            std::filesystem::rename(logPath, numbered(1), ec);
            if (ec)
                return fail("rename", logPath, ec);
            return true;
        }
    } // namespace

    void ReconfigureLoggingWithFile(const std::string& logFilePath)
    {
        // Get existing logger
        auto logger = spdlog::default_logger();
        const std::filesystem::path logPath{logFilePath};

        // Logging must never stop the application, and must never damage another instance's log.
        // - Another pserv instance (GUI or CLI) may hold the log file open. Renaming it then fails,
        //   and the throwing std::filesystem overloads used to terminate the process before main()
        //   did anything.
        // - Attaching a rotating sink to a file another process holds is not safe either: when that
        //   sink reaches its size limit, its rename fails and spdlog reopens the file truncated,
        //   destroying the other instance's active log.
        // So: if the file is held elsewhere, or rotation fails, this run logs without a file sink.
        if (IsHeldOpenByAnotherProcess(logPath))
        {
            logger->warn("File logging disabled for this run: '{}' is open in another process (another pserv instance?)", logFilePath);
            return;
        }
        if (!RotateLogFiles(logPath, *logger))
        {
            logger->warn("File logging disabled for this run: log rotation failed");
            return;
        }

        try
        {
            // NDJSON format - 10MB max size, keep 10 backups
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFilePath, 1024 * 1024 * 10, 10);
            file_sink->set_level(spdlog::level::debug);
            file_sink->set_formatter(std::make_unique<NdjsonFormatter>());
            logger->sinks().push_back(file_sink);
            // The file sink buffers: without these, the last lines before an exit or a crash were
            // lost. Warnings and errors are written at once, everything else within 2 seconds;
            // BaseApp flushes once more when it shuts down.
            logger->flush_on(spdlog::level::warn);
            spdlog::flush_every(std::chrono::seconds(2));
        }
        catch (const std::exception &e)
        {
            logger->warn("File logging disabled: cannot open '{}': {}", logFilePath, e.what());
        }
    }
} // namespace pserv::utils
