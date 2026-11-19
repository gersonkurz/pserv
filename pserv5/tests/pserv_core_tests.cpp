// pserv_core_tests: executable checks over the shared core. Run by `just test`; exit code 0 = pass.
//
// Checks run against the real controllers and live system data, so the data differs between
// runs and machines; every check is written to hold for any data.
#include "precomp.h"
#include <iostream>
#include <core/data_controller.h>
#include <core/data_controller_library.h>
#include <core/data_object.h>
#include <core/data_object_column.h>
#include <core/exporters/exporter_registry.h>
#include <models/scheduled_task_info.h>
#include <windows_api/scheduled_task_manager.h>
#include <utils/logging.h>
#include <spdlog/sinks/null_sink.h>

namespace
{
    using namespace pserv;

    int g_failures = 0;
    int g_checks = 0;

    template <typename... Args> void Fail(std::format_string<Args...> fmt, Args &&...args)
    {
        ++g_failures;
        std::cout << "  FAIL: " << std::format(fmt, std::forward<Args>(args)...) << "\n";
    }

    void Check(bool bOk, std::string_view what)
    {
        ++g_checks;
        if (!bOk)
            Fail("{}", what);
    }

    // A controller whose columns are deliberately out of property-id order, to prove the
    // contract check detects the class of bug behind the Processes column mismatch (F1).
    enum class FakeProperty
    {
        A = 0,
        B,
        C
    };

    class MisorderedController final : public DataController
    {
    public:
        MisorderedController()
            : DataController{"Misordered", "Item",
                  {{FakeProperty::A, "A", "A"}, {FakeProperty::C, "C", "C"}, {FakeProperty::B, "B", "B"}}}
        {
        }
        void Refresh(bool) override {}
        VisualState GetVisualState(const DataObject *) const override { return VisualState::Normal; }
        std::vector<const DataAction *> GetActions(const DataObject *) const override { return {}; }
    };

    // The Task Scheduler functions used to call CoUninitialize() on their error paths while COM
    // pointers were still alive; the pointers' later Release() crashed with 0xC0000005. A task
    // path that cannot exist forces exactly those error paths (GetTask / DeleteTask fail) without
    // touching any real task. Must run on a thread where COM is not already initialized, so it
    // runs first.
    void TestComErrorPathsDoNotCrash()
    {
        std::cout << "[Task Scheduler error paths release COM objects before CoUninitialize]\n";
        auto *task = DBG_NEW ScheduledTaskInfo{"pserv_core_tests"};
        task->SetValues(R"(\pserv_core_tests\this-task-does-not-exist-4d1f0c2e)", "", "", "", "", "", false,
            ScheduledTaskState::Unknown);
        Check(!ScheduledTaskManager::RunTask(task), "RunTask reported success for a nonexistent task");
        Check(!ScheduledTaskManager::SetTaskEnabled(task, false), "SetTaskEnabled reported success for a nonexistent task");
        Check(!ScheduledTaskManager::DeleteTask(task), "DeleteTask reported success for a nonexistent task");
        task->Release(REFCOUNT_DEBUG_ARGS);
    }

    std::string ReadWholeFile(const std::filesystem::path &p)
    {
        // Opened with full sharing, so it works while the test's "other instance" handle is open.
        wil::unique_hfile f{CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (!f)
            return "<cannot open>";
        std::string data;
        char buf[65536];
        DWORD n = 0;
        while (ReadFile(f.get(), buf, sizeof(buf), &n, nullptr) && n > 0)
            data.append(buf, n);
        return data;
    }

    // Another pserv instance holding the log must neither crash this one nor lose its log. A
    // rotating sink attached to a file someone else holds truncates it when its own rotation
    // fails at the size limit (spdlog reopens the file truncated). So, while another handle holds
    // the file, ReconfigureLoggingWithFile must attach no sink, and logging past the 10 MB
    // threshold must leave the held file byte-for-byte unchanged. Once the handle is released, the
    // same call must rotate and attach a sink (positive control). Uses a temp directory only.
    void TestLogFileHeldByAnotherInstance()
    {
        std::cout << "[log file held by another instance: no sink, no truncation past 10 MB; positive control]\n";
        const auto dir = std::filesystem::temp_directory_path() / std::format("pserv_core_tests_log_{}", GetCurrentProcessId());
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        const auto logPath = dir / "pserv5.log";
        const std::string foreignContent = "{\"msg\":\"other instance's active log\"}\n";

        auto previous = spdlog::default_logger();
        auto testLogger = std::make_shared<spdlog::logger>("test", std::make_shared<spdlog::sinks::null_sink_mt>());
        testLogger->set_level(spdlog::level::info);
        spdlog::set_default_logger(testLogger);
        {
            // The "other instance": same sharing as spdlog's _SH_DENYNO (read/write, not delete).
            wil::unique_hfile holder{CreateFileW(logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
            Check(static_cast<bool>(holder), "could not create the held log file");
            DWORD written = 0;
            WriteFile(holder.get(), foreignContent.data(), static_cast<DWORD>(foreignContent.size()), &written, nullptr);

            const size_t sinksBefore = testLogger->sinks().size();
            utils::ReconfigureLoggingWithFile(logPath.string());
            Check(testLogger->sinks().size() == sinksBefore, "a file sink was attached although another process holds the log");

            const std::string line(1000, 'x');
            for (int i = 0; i < 12 * 1024; ++i) // ~12 MB, beyond the sink's 10 MB rotation threshold
                testLogger->info("{}", line);
            testLogger->flush();

            Check(ReadWholeFile(logPath) == foreignContent, "the held log file was modified or truncated");
            Check(!std::filesystem::exists(dir / "pserv5.1.log"), "a rotation file appeared although the log is held");
        }
        {
            // Positive control: nothing holds the file now, so rotation and a sink must happen.
            const size_t sinksBefore = testLogger->sinks().size();
            utils::ReconfigureLoggingWithFile(logPath.string());
            Check(testLogger->sinks().size() == sinksBefore + 1, "no file sink was attached for a free log file");
            Check(ReadWholeFile(dir / "pserv5.1.log") == foreignContent, "the free log file was not rotated to pserv5.1.log");
        }
        spdlog::set_default_logger(previous);
        testLogger.reset(); // closes the file sink
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    void TestContractCheckDetectsMisorder()
    {
        std::cout << "[contract check detects a misordered controller]\n";
        MisorderedController c;
        int mismatch = c.FindColumnPropertyIdMismatch();
        Check(mismatch == 1, std::format("expected first mismatch at column 1, got {}", mismatch));
    }

    void TestColumnPropertyIdsInOrder(const std::vector<DataController *> &controllers)
    {
        std::cout << "[every column shows the property with its own index]\n";
        for (auto *c : controllers)
        {
            int mismatch = c->FindColumnPropertyIdMismatch();
            Check(mismatch == -1,
                std::format("{}: column {} ('{}') shows property {}", c->GetControllerName(), mismatch,
                    mismatch >= 0 ? c->GetColumns()[mismatch].DisplayName : "",
                    mismatch >= 0 ? c->GetColumns()[mismatch].PropertyId : -1));
        }
    }

    std::string_view KindName(const PropertyValue &v)
    {
        switch (v.index())
        {
        case 0: return "empty";
        case 1: return "int64";
        case 2: return "uint64";
        default: return "string";
        }
    }

    // Typed values must be sortable the way the column's DataType says. Time columns are
    // formatted strings today (e.g. ProcessInfo::GetTypedProperty), so they accept strings.
    bool KindFits(ColumnDataType type, const PropertyValue &v)
    {
        if (std::holds_alternative<std::monostate>(v))
            return true;
        switch (type)
        {
        case ColumnDataType::String:
            return std::holds_alternative<std::string>(v);
        case ColumnDataType::Integer:
        case ColumnDataType::UnsignedInteger:
        case ColumnDataType::Size:
            return std::holds_alternative<int64_t>(v) || std::holds_alternative<uint64_t>(v);
        case ColumnDataType::Time:
            return true;
        }
        return false;
    }

    void TestLiveProperties(const std::vector<DataController *> &controllers)
    {
        std::cout << "[live data: typed kinds fit column types; string columns display their typed value]\n";
        constexpr size_t SAMPLE = 200;
        for (auto *c : controllers)
        {
            // Printed before the refresh, so a crash inside it still names the controller.
            std::cout << std::format("  {}: refreshing...\n", c->GetControllerName());
            auto t0 = std::chrono::steady_clock::now();
            c->Refresh(false);
            auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            const auto &objects = c->GetDataObjects();
            const auto &columns = c->GetColumns();
            std::cout << std::format("  {}: {} objects, {} columns, refresh {:.0f} ms\n", c->GetControllerName(),
                objects.GetSize(), columns.size(), ms);
            size_t n = 0;
            int reported = 0;
            for (const auto *obj : objects)
            {
                if (n++ >= SAMPLE)
                    break;
                for (size_t i = 0; i < columns.size(); ++i)
                {
                    const auto &col = columns[i];
                    std::string display = obj->GetProperty(static_cast<int>(i));
                    PropertyValue typed = obj->GetTypedProperty(static_cast<int>(i));
                    ++g_checks;
                    if (!KindFits(col.DataType, typed) && reported < 5)
                    {
                        ++reported;
                        Fail("{} '{}' column {} '{}': typed value is {}, column type does not fit (display '{}')",
                            c->GetControllerName(), obj->GetItemName(), i, col.DisplayName, KindName(typed), display);
                    }
                    if (col.DataType == ColumnDataType::String && std::holds_alternative<std::string>(typed))
                    {
                        ++g_checks;
                        if (std::get<std::string>(typed) != display && reported < 5)
                        {
                            ++reported;
                            Fail("{} '{}' column {} '{}': display '{}' != typed '{}'", c->GetControllerName(),
                                obj->GetItemName(), i, col.DisplayName, display, std::get<std::string>(typed));
                        }
                    }
                }
            }
        }
    }

    // Exporters used to self-register from static initializers, which a static library drops when
    // nothing references their object file. The registry now registers them itself; this proves
    // they are present in a binary that links pserv_core.lib and that they produce output.
    void TestExportersRegistered(const std::vector<DataController *> &controllers)
    {
        std::cout << "[built-in exporters are registered and export live objects]\n";
        auto &registry = ExporterRegistry::Instance();
        Check(registry.FindExporter("JSON") != nullptr, "JSON exporter not registered");
        Check(registry.FindExporter("Plain Text") != nullptr, "Plain Text exporter not registered");
        const DataController *services = controllers.front(); // already refreshed by TestLiveProperties
        std::vector<DataObject *> sample;
        for (auto *obj : services->GetDataObjects())
        {
            if (sample.size() == 3)
                break;
            sample.push_back(obj);
        }
        for (auto *exporter : registry.GetExporters())
        {
            std::string out = exporter->ExportMultiple(sample, services->GetColumns());
            Check(!sample.empty() && !out.empty(), std::format("{} exporter produced no output", exporter->GetFormatName()));
        }
    }
}

int main()
{
    // Unbuffered: with stdout redirected, a crash would otherwise discard everything printed so far
    // and leave no hint of where it happened.
    std::cout << std::unitbuf;
    spdlog::set_level(spdlog::level::off); // managers log access-denied noise when not elevated
    std::cout << "pserv_core_tests\n";

    TestComErrorPathsDoNotCrash();
    TestLogFileHeldByAnotherInstance();
    TestContractCheckDetectsMisorder();

    pserv::DataControllerLibrary library;
    const auto &controllers = library.GetDataControllers();
    TestColumnPropertyIdsInOrder(controllers);
    TestLiveProperties(controllers);
    TestExportersRegistered(controllers);

    std::cout << std::format("{} checks, {} failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
