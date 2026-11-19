#include "pch.h"
#include "self_test.h"
#include "MainWindow.xaml.h"
#include <utils/string_utils.h>
#include <psapi.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace pserv6
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        double Ms(Clock::duration d)
        {
            return std::chrono::duration<double, std::milli>(d).count();
        }

        double Percentile(std::vector<double> v, double p)
        {
            if (v.empty())
                return 0;
            std::sort(v.begin(), v.end());
            const size_t i = std::min(v.size() - 1, static_cast<size_t>(p * static_cast<double>(v.size() - 1) + 0.5));
            return v[i];
        }

        ScrollViewer FindScrollViewer(DependencyObject const &root)
        {
            const int n = Media::VisualTreeHelper::GetChildrenCount(root);
            for (int i = 0; i < n; ++i)
            {
                auto child = Media::VisualTreeHelper::GetChild(root, i);
                if (auto sv = child.try_as<ScrollViewer>())
                    return sv;
                if (auto sv = FindScrollViewer(child))
                    return sv;
            }
            return nullptr;
        }

        bool IsElevated()
        {
            wil::unique_handle token;
            TOKEN_ELEVATION elevation{};
            DWORD size = 0;
            return OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put()) &&
                   GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
        }

        std::vector<std::string> Split(const std::string &s)
        {
            std::vector<std::string> parts;
            std::string item;
            std::istringstream in{s};
            while (std::getline(in, item, ','))
            {
                if (!item.empty())
                    parts.push_back(item);
            }
            return parts;
        }
    } // namespace

    std::unique_ptr<SelfTest> SelfTest::FromCommandLine(winrt::pserv6::implementation::MainWindow &window)
    {
        int argc = 0;
        wil::unique_hlocal_ptr<LPWSTR> argv{CommandLineToArgvW(GetCommandLineW(), &argc)};
        if (!argv)
            return nullptr;
        bool bSelfTest = false;
        std::vector<std::string> views;
        int seconds = 3;
        std::filesystem::path log = std::filesystem::temp_directory_path() / "pserv6-selftest.log";
        for (int i = 1; i < argc; ++i)
        {
            const std::wstring arg{argv.get()[i]};
            const bool bHasValue = i + 1 < argc && argv.get()[i + 1][0] != L'-';
            if (arg == L"--selftest")
            {
                bSelfTest = true;
                if (bHasValue)
                    views = Split(pserv::utils::WideToUtf8(argv.get()[++i]));
            }
            else if (arg == L"--seconds" && bHasValue)
                seconds = std::clamp(_wtoi(argv.get()[++i]), 1, 60);
            else if (arg == L"--log" && bHasValue)
                log = argv.get()[++i];
        }
        if (!bSelfTest)
            return nullptr;
        if (views.empty())
        {
            for (auto *controller : window.Controllers())
                views.emplace_back(controller->GetControllerName());
        }
        return std::make_unique<SelfTest>(window, std::move(views), seconds, std::move(log));
    }

    SelfTest::SelfTest(winrt::pserv6::implementation::MainWindow &window, std::vector<std::string> views, int seconds, std::filesystem::path log)
        : m_window{window},
          m_views{std::move(views)},
          m_seconds{seconds},
          m_logPath{std::move(log)},
          m_log{m_logPath, std::ios::out | std::ios::trunc}
    {
        Line(std::format("pserv6 self-test: {} view(s), {} s each, elevated={}", m_views.size(), m_seconds, IsElevated()));
        m_timeout = m_window.DispatcherQueue().CreateTimer();
        m_timeout.IsRepeating(false);
        m_timeout.Tick([this](auto &&, auto &&)
            {
                if (m_stage == Stage::WaitPublish || m_stage == Stage::WaitFrame)
                {
                    m_current.error = "timed out waiting for the first publish/frame";
                    EndMeasure();
                }
            });
        // Start once the window has been laid out
        m_window.DispatcherQueue().TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [this] { Start(); });
    }

    SelfTest::~SelfTest()
    {
        if (m_renderingToken)
            Media::CompositionTarget::Rendering(m_renderingToken);
    }

    void SelfTest::Line(const std::string &text)
    {
        m_log << text << '\n';
        m_log.flush();
        spdlog::info("selftest: {}", text);
    }

    void SelfTest::Start()
    {
        m_renderingToken = Media::CompositionTarget::Rendering([this](auto &&, auto &&) { OnRendering(); });
        NextView();
    }

    void SelfTest::NextView()
    {
        if (m_index >= m_views.size())
        {
            Finish();
            return;
        }
        m_current = ViewResult{};
        m_current.name = m_views[m_index++];
        m_window.Grid().m_stats = {};
        m_scroller = nullptr;
        m_stage = Stage::WaitPublish;
        m_switchAt = Clock::now();
        m_timeout.Interval(std::chrono::seconds(60));
        m_timeout.Start();
        if (!m_window.SelectView(m_current.name))
        {
            m_current.error = "no such view";
            EndMeasure();
        }
    }

    void SelfTest::OnPublished(double uiMs)
    {
        const auto now = Clock::now();
        if (m_stage == Stage::WaitPublish)
        {
            m_current.switchToPublishMs = Ms(now - m_switchAt);
            m_current.firstPublishUiMs = uiMs;
            m_stage = Stage::WaitFrame;
        }
        else if (m_stage == Stage::Measure)
        {
            m_current.publishUiMs.push_back(uiMs);
        }
    }

    void SelfTest::OnRendering()
    {
        const auto now = Clock::now();
        switch (m_stage)
        {
        case Stage::WaitFrame:
            m_current.switchToFrameMs = Ms(now - m_switchAt);
            m_timeout.Stop();
            m_stage = Stage::Measure;
            m_lastFrame = now;
            m_measureEnd = now + std::chrono::seconds(m_seconds);
            m_nextRefresh = now + std::chrono::seconds(2);
            m_scrollOffset = 0;
            m_scrollDirection = 1;
            m_scroller = FindScrollViewer(m_window.RowList());
            break;
        case Stage::Measure:
        {
            m_current.frameMs.push_back(Ms(now - m_lastFrame));
            m_lastFrame = now;
            // The window ends once the time is up *and* at least one refresh has been published, so
            // every view's refresh path is measured (Modules' enumeration alone takes ~2 s).
            if (now >= m_measureEnd && !m_current.publishUiMs.empty())
            {
                EndMeasure();
                return;
            }
            if (now >= m_measureEnd + std::chrono::seconds(30))
            {
                m_current.error = "no refresh was published within 30 s after the measuring window";
                EndMeasure();
                return;
            }
            if (now >= m_nextRefresh)
            {
                // A manual request: three views do not auto-refresh (SupportsAutoRefresh), and
                // the publish path is the same for both kinds.
                m_nextRefresh = now + std::chrono::seconds(2);
                m_window.RequestRefresh(false);
            }
            // Continuous scrolling: three rows per frame, bouncing at either end
            if (auto sv = m_scroller)
            {
                const double max = sv.ScrollableHeight();
                m_scrollOffset += m_scrollDirection * 72.0;
                if (m_scrollOffset >= max || m_scrollOffset <= 0)
                {
                    m_scrollOffset = std::clamp(m_scrollOffset, 0.0, max);
                    m_scrollDirection = -m_scrollDirection;
                }
                sv.ChangeView(nullptr, m_scrollOffset, nullptr, true);
            }
            break;
        }
        default:
            break;
        }
    }

    void SelfTest::EndMeasure()
    {
        m_timeout.Stop();
        auto *pController = m_window.CurrentController();
        m_current.rows = pController ? pController->GetDataObjects().GetSize() : 0;
        if (m_current.error.empty() && pController && m_window.Grid().RowCount() != m_current.rows)
        {
            m_current.error = std::format("grid shows {} rows, controller has {}", m_window.Grid().RowCount(), m_current.rows);
        }

        // Budgets: plan section 7 (targets; binding from Phase 4 on)
        const bool bLarge = m_current.rows > 2000;
        const double publishBudget = bLarge ? 16.0 : 8.0;
        // Nothing measured is reported as unmeasured, never as 0 ms "within".
        const bool bPublishMeasured = !m_current.publishUiMs.empty();
        const bool bFramesMeasured = !m_current.frameMs.empty();
        const double maxPublish = bPublishMeasured ? *std::max_element(m_current.publishUiMs.begin(), m_current.publishUiMs.end()) : 0;
        const double p95 = Percentile(m_current.frameMs, 0.95);
        const double maxFrame = bFramesMeasured ? *std::max_element(m_current.frameMs.begin(), m_current.frameMs.end()) : 0;
        std::vector<std::string> over, unmeasured;
        if (bPublishMeasured && maxPublish > publishBudget)
            over.push_back(std::format("publish>{}ms", publishBudget));
        if (!bPublishMeasured)
            unmeasured.push_back("publish");
        if (!bLarge) // the frame and switch budgets cover views of up to 2,000 rows
        {
            if (m_current.switchToFrameMs < 0)
                unmeasured.push_back("switch");
            else if (m_current.switchToFrameMs > 150)
                over.push_back("switch>150ms");
            if (!bFramesMeasured)
                unmeasured.push_back("frames");
            else
            {
                if (p95 > 17)
                    over.push_back("p95>17ms");
                if (maxFrame > 50)
                    over.push_back("max>50ms");
            }
        }
        auto join = [](const std::vector<std::string> &items)
        {
            std::string text;
            for (const auto &item : items)
                text += (text.empty() ? "" : ",") + item;
            return text;
        };
        std::string budgets = over.empty() ? "within" : "over:" + join(over);
        if (!unmeasured.empty())
            budgets += " unmeasured:" + join(unmeasured);
        if (bLarge)
            budgets += " (frame/switch budgets n/a: >2000 rows)";

        const auto &stats = m_window.Grid().m_stats;
        Line(std::format("{:<22} rows={:>6} switch->publish={:>7.1f}ms switch->frame={:>7.1f}ms firstPublishUI={:.2f}ms refreshes={} refreshUI max={} "
                         "frames={} p95={:.1f}ms max={:.1f}ms grid: refresh {}x/{} resets/{:.1f}ms, containers {}x/{:.1f}ms budgets={}{}",
            m_current.name, m_current.rows, m_current.switchToPublishMs, m_current.switchToFrameMs, m_current.firstPublishUiMs, m_current.publishUiMs.size(),
            bPublishMeasured ? std::format("{:.2f}ms", maxPublish) : std::string{"-"}, m_current.frameMs.size(), p95, maxFrame, stats.refreshes, stats.resets,
            stats.refreshMs, stats.containers, stats.containerMs, budgets, m_current.error.empty() ? "" : " ERROR: " + m_current.error));
        m_results.push_back(std::move(m_current));
        m_stage = Stage::Idle;
        // Let the dispatcher breathe before the next switch
        m_window.DispatcherQueue().TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [this] { NextView(); });
    }

    void SelfTest::Finish()
    {
        m_stage = Stage::Done;
        PROCESS_MEMORY_COUNTERS pmc{};
        pmc.cb = sizeof(pmc);
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        Line(std::format("working set {:.1f} MB (peak {:.1f} MB)", pmc.WorkingSetSize / 1048576.0, pmc.PeakWorkingSetSize / 1048576.0));
        // Shutdown with work in flight: the window must join the worker before it destroys the
        // coordinator the worker's task calls back into.
        m_window.RequestRefresh(false);
        Line(std::format("closing with a refresh in flight: {}", m_window.IsRefreshInFlight()));
        const bool bFailed = std::any_of(m_results.begin(), m_results.end(), [](const ViewResult &r) { return !r.error.empty(); });
        // Written before the shutdown it precedes: the wrapper (scripts/selftest.ps1) also requires
        // exit code 0, so a crash while closing fails the run.
        Line(bFailed ? "RESULT: FAIL" : "RESULT: PASS");
        m_window.Close();
    }
} // namespace pserv6
