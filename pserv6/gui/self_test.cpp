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
            // Second pass: switch back to every (now loaded) view. This is the plan's "view
            // switch" budget; the first visit above also contains the first enumeration.
            if (!m_bRevisiting && m_results.size() > 1)
            {
                m_bRevisiting = true;
                m_revisitIndex = 0;
                NextRevisit();
                return;
            }
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

    void SelfTest::NextRevisit()
    {
        while (m_revisitIndex < m_results.size() && !m_results[m_revisitIndex].error.empty())
            ++m_revisitIndex;
        if (m_revisitIndex >= m_results.size())
        {
            std::string text, over;
            for (const auto &r : m_results)
            {
                if (r.revisitMs < 0)
                    continue;
                text += std::format(" {} {:.0f}ms,", r.name, r.revisitMs);
                if (r.rows <= 2000 && r.revisitMs > 150)
                    over += " " + r.name;
            }
            if (!text.empty())
                text.pop_back();
            Line(std::format("switching back to a loaded view (select -> second rendered frame):{}", text));
            Line(std::format("view-switch budget (150 ms, views up to 2,000 rows): {}", over.empty() ? "within" : "over:" + over));
            Finish();
            return;
        }
        m_stage = Stage::Revisit;
        m_frameCountdown = 2;
        m_switchAt = Clock::now();
        m_window.SelectView(m_results[m_revisitIndex].name);
    }

    void SelfTest::OnPublished(double uiMs)
    {
        const auto now = Clock::now();
        if (m_stage == Stage::WaitPublish)
        {
            m_current.switchToPublishMs = Ms(now - m_switchAt);
            m_current.firstPublishUiMs = uiMs;
            m_frameCountdown = 2;
            m_stage = Stage::WaitFrame;
        }
        else if (m_stage == Stage::Measure)
        {
            m_current.publishUiMs.push_back(uiMs);
            m_current.publishAtMs.push_back(Ms(now - m_measureStart));
        }
    }

    void SelfTest::OnRendering()
    {
        const auto now = Clock::now();
        switch (m_stage)
        {
        case Stage::Revisit:
            if (--m_frameCountdown > 0)
                break;
            m_results[m_revisitIndex].revisitMs = Ms(now - m_switchAt);
            ++m_revisitIndex;
            m_stage = Stage::Idle;
            m_window.DispatcherQueue().TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [this] { NextRevisit(); });
            break;
        case Stage::WaitFrame:
            if (--m_frameCountdown > 0)
                break; // the first Rendering after the publish may precede its layout pass
            m_current.switchToFrameMs = Ms(now - m_switchAt);
            m_timeout.Stop();
            m_stage = Stage::Measure;
            m_lastFrame = now;
            m_measureStart = now;
            m_measureEnd = now + std::chrono::seconds(m_seconds);
            m_nextRefresh = now + std::chrono::seconds(2);
            m_scrollOffset = 0;
            m_scrollDirection = 1;
            m_scroller = FindScrollViewer(m_window.RowList());
            break;
        case Stage::Measure:
        {
            m_current.frameMs.push_back(Ms(now - m_lastFrame));
            m_current.frameAtMs.push_back(Ms(now - m_measureStart));
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

    // Grid behaviour on the first view with enough rows (Phase 4): the column layout survives a
    // reload through the settings, and the selection survives a re-sort (a ReplaceAll). Leaves
    // layout and sort as they were; a self-test run never saves the settings.
    std::string SelfTest::RunGridChecks(pserv::DataController &controller)
    {
        auto &grid = m_window.Grid();
        const auto order = grid.DisplayOrder();
        const auto widths = grid.Widths();
        const int sortColumn = controller.GetLastSortColumn();
        const bool bSortAscending = controller.IsLastSortAscending();
        if (order.size() < 2)
            return {};

        // Every header generation discarded below must be released (checked in Finish(), when
        // the rebuilds are long done): no event handler may keep its element alive.
        auto keep = [this, &grid]
        {
            for (auto &ref : grid.HeaderCellRefs())
                m_discardedHeaderCells.push_back(std::move(ref));
        };

        // 1. Layout round trip: move the first column one to the right, widen it, reload.
        const int moved = order[0];
        keep();
        grid.MoveColumn(0, 1);
        // The Sort menu follows the new display order
        const auto firstMenuItem = m_window.SortButton().Flyout().as<winrt::Microsoft::UI::Xaml::Controls::MenuFlyout>().Items().GetAt(0)
                                       .as<winrt::Microsoft::UI::Xaml::Controls::MenuFlyoutItem>().Text();
        const bool bMenuFollows = winrt::to_string(firstMenuItem) == controller.GetColumns()[static_cast<size_t>(grid.DisplayOrder()[0])].DisplayName;
        grid.SetColumnWidth(moved, widths[static_cast<size_t>(moved)] + 37);
        keep();
        grid.SetController(&controller); // re-reads the layout from the settings
        const bool bLayoutKept = grid.DisplayOrder().size() == order.size() && grid.DisplayOrder()[1] == moved &&
                                 grid.Widths()[static_cast<size_t>(moved)] == std::clamp(widths[static_cast<size_t>(moved)] + 37, 30.0, 2000.0);
        keep();
        grid.MoveColumn(1, 0);
        grid.SetColumnWidth(moved, widths[static_cast<size_t>(moved)]);
        bool bLayoutRestored = grid.DisplayOrder() == order && grid.Widths() == widths;
        // Hide that column and show it again: one header cell less, the stored order untouched
        if (const size_t shown = grid.ShownColumnCount(); shown > 1 && !grid.IsColumnHidden(moved))
        {
            keep();
            grid.SetColumnHidden(moved, true);
            const bool bHidden = grid.ShownColumnCount() == shown - 1 && grid.HeaderCellRefs().size() == shown - 1 && grid.DisplayOrder() == order;
            keep();
            grid.SetColumnHidden(moved, false);
            bLayoutRestored = bLayoutRestored && bHidden && grid.ShownColumnCount() == shown && grid.DisplayOrder() == order;
        }

        // 2. Selection and scroll position across a re-sort (rows rewritten in place): scroll down, select
        // rows 1 and 3, reverse the sort, compare.
        grid.ScrollTo(10 * 24.0);
        const double offsetBefore = grid.VerticalOffset();
        m_window.RowList().SelectRange(winrt::Microsoft::UI::Xaml::Data::ItemIndexRange{1, 1});
        m_window.RowList().SelectRange(winrt::Microsoft::UI::Xaml::Data::ItemIndexRange{3, 1});
        auto before = grid.GetSelection();
        const int resetsBefore = grid.m_stats.resets;
        grid.SortBy(0);
        grid.SortBy(0); // twice: the second toggles the direction, so the order surely changes
        auto after = grid.GetSelection();
        const bool bReset = grid.m_stats.resets == resetsBefore; // in place: the list is not reset
        std::sort(before.begin(), before.end());
        std::sort(after.begin(), after.end());
        const bool bSelectionKept = before.size() == 2 && before == after;

        // The scroll position is read once the dispatcher is idle, so a late adjustment by the
        // ListView after the reset would be seen. Then the reviewer's sequence: scroll to the
        // top and re-sort again; the offset must stay 0 (no older restore may win). The next
        // view switch waits for the last continuation (m_bGridCheckPending).
        const std::string name{controller.GetControllerName()};
        auto dispatcher = m_window.DispatcherQueue();
        dispatcher.TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
            [this, dispatcher, &controller, name, bLayoutKept, bLayoutRestored, bMenuFollows, bReset, bSelectionKept, offsetBefore, sortColumn, bSortAscending]
            {
                auto &grid = m_window.Grid();
                const double offsetAfter = grid.VerticalOffset();
                const bool bScrollKept = offsetBefore > 0 && std::abs(offsetAfter - offsetBefore) < 1.0;
                grid.ScrollTo(0);
                grid.SortBy(0);
                dispatcher.TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                    [this, &controller, name, bLayoutKept, bLayoutRestored, bMenuFollows, bReset, bSelectionKept, bScrollKept, offsetBefore, offsetAfter, sortColumn, bSortAscending]
                    {
                        auto &grid = m_window.Grid();
                        const double offsetTop = grid.VerticalOffset();
                        const bool bTopKept = offsetTop < 1.0;
                        m_window.RowList().DeselectRange(winrt::Microsoft::UI::Xaml::Data::ItemIndexRange{0, static_cast<uint32_t>(grid.RowCount())});
                        // Restore the sort
                        if (sortColumn >= 0)
                            controller.Sort(sortColumn, bSortAscending);
                        for (auto &ref : grid.HeaderCellRefs())
                            m_discardedHeaderCells.push_back(std::move(ref));
                        grid.SetController(&controller);
                        grid.ScrollTo(0);
                        Line(std::format("grid checks on {}: layout round trip {}, layout restored {}, sort menu follows the column order {}, "
                                         "re-sort without a reset {}, selection kept {}, scroll position kept {} ({:.0f} -> {:.0f}), "
                                         "top kept after a reset at the top {} ({:.0f})",
                            name, bLayoutKept, bLayoutRestored, bMenuFollows, bReset, bSelectionKept, bScrollKept, offsetBefore, offsetAfter, bTopKept, offsetTop));
                        if (!bLayoutKept || !bLayoutRestored || !bMenuFollows || !bReset || !bSelectionKept || !bScrollKept || !bTopKept)
                            m_gridCheckError = "grid checks failed";
                        // Only now go on: the checks re-attach this view's rows
                        m_bGridCheckPending = false;
                        m_window.DispatcherQueue().TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [this] { NextView(); });
                    });
            });
        return {};
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
        if (m_current.error.empty() && pController && !m_bGridChecked && m_current.rows >= 5)
        {
            m_bGridChecked = true;
            m_bGridCheckPending = true;
            m_current.error = RunGridChecks(*pController);
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
                over.push_back("first-visit>150ms");
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
        // Where the long frames are: each over 50 ms with its time, and whether a refresh was
        // published during it (P) or it is one of the first frames after the switch.
        std::string longFrames;
        for (size_t i = 0; i < m_current.frameMs.size(); ++i)
        {
            if (m_current.frameMs[i] <= 50)
                continue;
            const double end = m_current.frameAtMs[i], begin = end - m_current.frameMs[i];
            const bool bPublish = std::any_of(m_current.publishAtMs.begin(), m_current.publishAtMs.end(), [&](double at) { return at >= begin - 1 && at <= end + 1; });
            longFrames += std::format(" {:.0f}ms@{:.0f}{}", m_current.frameMs[i], end, bPublish ? "P" : "");
        }
        if (!longFrames.empty())
            Line(std::format("{:<22} frames over 50 ms (duration@end, P = a publish in it):{}", "", longFrames));
        m_results.push_back(std::move(m_current));
        m_stage = Stage::Idle;
        // Let the dispatcher breathe before the next switch (the grid checks go on themselves)
        if (!m_bGridCheckPending)
            m_window.DispatcherQueue().TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [this] { NextView(); });
    }

    void SelfTest::Finish()
    {
        m_stage = Stage::Done;
        PROCESS_MEMORY_COUNTERS pmc{};
        pmc.cb = sizeof(pmc);
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        Line(std::format("working set {:.1f} MB (peak {:.1f} MB)", pmc.WorkingSetSize / 1048576.0, pmc.PeakWorkingSetSize / 1048576.0));
        if (!m_gridCheckError.empty() && !m_results.empty())
            m_results.front().error = m_gridCheckError;
        const auto alive = std::count_if(m_discardedHeaderCells.begin(), m_discardedHeaderCells.end(), [](auto const &ref) { return ref.get() != nullptr; });
        Line(std::format("discarded header cells still alive: {} of {}", alive, m_discardedHeaderCells.size()));
        if (alive > 0 && !m_results.empty() && m_results.front().error.empty())
            m_results.front().error = "discarded header cells are still alive (a reference cycle?)";
        const std::string themeError = m_window.CheckThemes();
        Line(std::format("Base16 themes: {} applied and removed again{}", m_window.ThemeCount(), themeError.empty() ? "" : " ERROR: " + themeError));
        if (!themeError.empty() && !m_results.empty() && m_results.front().error.empty())
            m_results.front().error = themeError;
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
