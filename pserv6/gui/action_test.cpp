#include "pch.h"
#include "action_test.h"
#include "MainWindow.xaml.h"
#include <actions/common_actions.h>
#include <core/data_controller.h>
#include <core/data_object.h>
#include <utils/string_utils.h>

namespace pserv6
{
    namespace
    {
        constexpr wchar_t RunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        constexpr wchar_t ApprovedRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
        constexpr wchar_t WindowClass[] = L"pserv6SelfTestWindow";

        bool RegistryValueExists(HKEY root, const wchar_t *key, const std::wstring &name)
        {
            return RegGetValueW(root, key, name.c_str(), RRF_RT_ANY, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
        }

        bool IsElevated()
        {
            wil::unique_handle token;
            TOKEN_ELEVATION elevation{};
            DWORD size = 0;
            return OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put()) &&
                   GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated;
        }

        std::string ReadClipboard()
        {
            for (int attempt = 0; attempt < 10; ++attempt)
            {
                if (OpenClipboard(nullptr))
                {
                    std::string text;
                    if (HANDLE data = GetClipboardData(CF_UNICODETEXT))
                    {
                        if (const auto *p = static_cast<const wchar_t *>(GlobalLock(data)))
                        {
                            text = pserv::utils::WideToUtf8(p);
                            GlobalUnlock(data);
                        }
                    }
                    CloseClipboard();
                    return text;
                }
                Sleep(50);
            }
            return {};
        }

        /// Runs a console program without a window and waits for it (at most 30 s).
        DWORD RunProgram(std::wstring commandLine)
        {
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            wil::unique_process_information pi;
            if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
                return GetLastError();
            WaitForSingleObject(pi.hProcess, 30000);
            DWORD code = 1;
            GetExitCodeProcess(pi.hProcess, &code);
            return code;
        }
    } // namespace

    namespace
    {
        // Harmless actions for the remote and target-change guards: all they do is count and
        // record the machine they were dispatched for.
        class RecordingAction final : public pserv::DataAction
        {
        public:
            explicit RecordingAction(bool bLocalOnly)
                : pserv::DataAction{bLocalOnly ? "Self-test Recording (local only)" : "Self-test Recording", pserv::ActionVisibility::ContextMenu},
                  m_bLocalOnly{bLocalOnly}
            {
            }
            bool IsAvailableFor(const pserv::DataObject *) const override { return true; }
            bool RequiresLocalMachine() const override { return m_bLocalOnly; }
            void Execute(pserv::DataActionDispatchContext &ctx) const override
            {
                ++m_count;
                m_lastTarget = ctx.m_pController ? ctx.m_pController->GetPublishedTarget() : std::string{"<none>"};
            }
            const bool m_bLocalOnly;
            mutable int m_count{0};
            mutable std::string m_lastTarget;
        };
    } // namespace

    std::unique_ptr<ActionTest> ActionTest::FromCommandLine(winrt::pserv6::implementation::MainWindow &window)
    {
        int argc = 0;
        wil::unique_hlocal_ptr<LPWSTR> argv{CommandLineToArgvW(GetCommandLineW(), &argc)};
        if (!argv)
            return nullptr;
        bool bRequested = false;
        std::filesystem::path log = std::filesystem::temp_directory_path() / "pserv6-actiontest.log";
        for (int i = 1; i < argc; ++i)
        {
            const std::wstring arg{argv.get()[i]};
            if (arg == L"--selftest-actions")
                bRequested = true;
            else if (arg == L"--log" && i + 1 < argc)
                log = argv.get()[++i];
        }
        return bRequested ? std::make_unique<ActionTest>(window, std::move(log)) : nullptr;
    }

    ActionTest::ActionTest(winrt::pserv6::implementation::MainWindow &window, std::filesystem::path log)
        : m_window{window},
          m_log{log, std::ios::out | std::ios::trunc},
          m_logPath{std::move(log)}
    {
        m_testName = std::format("pserv6-selftest-{}-{}", GetCurrentProcessId(), GetTickCount64() % 1000000);
        m_testNameW = pserv::utils::Utf8ToWide(m_testName);
        Line(std::format("pserv6 action self-test: elevated={}, throwaway name {}", IsElevated(), m_testName));
        m_window.Actions().SetAutomation(true);
        m_window.DispatcherQueue().TryEnqueue(winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [this] { Run(); });
    }

    ActionTest::~ActionTest()
    {
        *m_alive = false;
        Cleanup();
    }

    void ActionTest::Line(const std::string &text)
    {
        m_log << text << '\n';
        m_log.flush();
        spdlog::info("actiontest: {}", text);
    }

    void ActionTest::Check(bool bOk, const std::string &what)
    {
        Line(std::format("{:<5} {}", bOk ? "ok" : "FAIL", what));
        if (!bOk)
            ++m_failures;
    }

    winrt::Windows::Foundation::IAsyncAction ActionTest::Pause(int milliseconds)
    {
        auto dispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        co_await winrt::resume_after(std::chrono::milliseconds(milliseconds));
        co_await wil::resume_foreground(dispatcher);
    }

    winrt::Windows::Foundation::IAsyncAction ActionTest::ShowView(std::string name)
    {
        // Show the view (or refresh it) and wait until no action runs and no refresh is in flight
        auto *pCurrent = m_window.CurrentController();
        if (!pCurrent || pCurrent->GetControllerName() != name)
            m_window.SelectView(name);
        else
            m_window.RequestRefresh(false);
        co_await Pause(150);
        for (int i = 0; i < 300; ++i)
        {
            auto *c = m_window.CurrentController();
            if (c && c->IsLoaded() && !m_window.IsRefreshInFlight() && !m_window.Actions().IsBusy())
                break;
            co_await Pause(100);
        }
    }

    winrt::Windows::Foundation::IAsyncAction ActionTest::Exec(std::string action, pserv::DataObject *pObject, std::map<std::string, std::string> parameters)
    {
        m_lastMessages.clear();
        m_bLastOffered = false;
        auto *pController = m_window.CurrentController();
        if (!pController || !pObject)
        {
            Check(false, std::format("{}: no object to run it on", action));
            co_return;
        }
        auto actions = pController->GetActions(pObject);
        pserv::AddCommonExportActions(actions);
        const pserv::DataAction *pAction = nullptr;
        for (const auto *a : actions)
        {
            if (!a->IsSeparator() && a->GetName() == action)
                pAction = a;
        }
        if (!pAction)
        {
            Check(false, std::format("{}: not offered for this object", action));
            co_return;
        }
        m_bLastOffered = true;
        if (!pAction->IsAvailableFor(pObject))
        {
            Check(false, std::format("{}: offered but not available", action));
            co_return;
        }
        ++m_executed;
        Line(std::format("      run '{}' ({}) on '{}'", action, pController->GetControllerName(), pObject->GetItemName()));
        m_window.Actions().ExecuteNow(pAction, pController, {pObject}, std::move(parameters));
        for (int i = 0; i < 300 && m_window.Actions().IsBusy(); ++i) // asynchronous operations
            co_await Pause(100);
        m_lastMessages = m_window.Actions().TakeMessages();
        for (const auto &message : m_lastMessages)
            Line(std::format("      message: {}", message));
    }

    pserv::DataObject *ActionTest::Find(std::string_view column, std::string_view value) const
    {
        auto *c = m_window.CurrentController();
        if (!c)
            return nullptr;
        const auto &columns = c->GetColumns();
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (columns[i].DisplayName != column)
                continue;
            for (auto *obj : c->GetDataObjects())
            {
                if (obj->GetProperty(static_cast<int>(i)) == value)
                    return obj;
            }
        }
        return nullptr;
    }

    std::string ActionTest::Property(pserv::DataObject *pObject, std::string_view column) const
    {
        auto *c = m_window.CurrentController();
        if (!c || !pObject)
            return {};
        const auto &columns = c->GetColumns();
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (columns[i].DisplayName == column)
                return pObject->GetProperty(static_cast<int>(i));
        }
        return {};
    }

    winrt::fire_and_forget ActionTest::Run()
    {
        std::weak_ptr<bool> alive = m_alive;
        auto gone = [&alive] { auto a = alive.lock(); return !a || !*a; };
        m_window.SetRemoteMachine({}); // this machine (in memory only: a test run never saves settings)
        co_await Pause(500);
        if (gone())
            co_return;

        // --- Environment Variables: Add User Variable, edit, Copy Name/Value, Properties, Delete
        {
            co_await ShowView("Environment Variables");
            const std::string variable = std::format("PSERV6_SELFTEST_{}_{}", GetCurrentProcessId(), GetTickCount64() % 1000000);
            if (RegistryValueExists(HKEY_CURRENT_USER, L"Environment", pserv::utils::Utf8ToWide(variable)))
            {
                Check(false, std::format("env: '{}' already exists: refusing to touch it", variable));
                co_return;
            }
            m_variableName = variable;
            // "Add User Variable" is offered on user-scope rows ("Add System Variable" on system ones)
            co_await Exec("Add User Variable", Find("Scope", "User"), {{"name", m_variableName}, {"value", "one"}});
            co_await ShowView("Environment Variables");
            auto *v = Find("Name", m_variableName);
            Check(v && Property(v, "Value") == "one", "env: Add User Variable created it with the value 'one'");
            if (v)
            {
                // The properties dialog's edit path: the controller's transaction
                auto *c = m_window.CurrentController();
                int valueColumn = -1;
                for (size_t i = 0; i < c->GetColumns().size(); ++i)
                    if (c->GetColumns()[i].DisplayName == "Value")
                        valueColumn = static_cast<int>(i);
                c->BeginPropertyEdits(v);
                const bool bCommitted = c->SetPropertyEdit(v, valueColumn, "two") && c->CommitPropertyEdits(v);
                c->InvalidatePendingRefreshes();
                Check(bCommitted, "env: properties edit (Value 'one' -> 'two') committed");
                co_await ShowView("Environment Variables");
                v = Find("Name", m_variableName);
                Check(v && Property(v, "Value") == "two", "env: the edited value is 'two' after a refresh");
                co_await Exec("Copy Name", Find("Name", m_variableName));
                Check(ReadClipboard() == m_variableName, "env: Copy Name put the name on the clipboard");
                co_await Exec("Copy Value", Find("Name", m_variableName));
                Check(ReadClipboard() == "two", "env: Copy Value put the value on the clipboard");
                co_await Exec("Properties...", Find("Name", m_variableName));
                Check(std::any_of(m_lastMessages.begin(), m_lastMessages.end(), [](auto &m) { return m.starts_with("properties requested"); }),
                    "env: Properties... dispatched to the host's properties dialog");
                co_await Exec("Delete", Find("Name", m_variableName));
                co_await ShowView("Environment Variables");
                const bool bGone = Find("Name", m_variableName) == nullptr;
                Check(bGone, "env: Delete removed the variable");
                if (bGone)
                    m_variableName.clear();
            }
        }
        if (gone())
            co_return;

        // --- Processes: Terminate Process (asynchronous) on a throwaway ping.exe
        {
            STARTUPINFOW si{};
            si.cb = sizeof(si);
            std::wstring cmd = L"\"C:\\Windows\\System32\\PING.EXE\" -n 120 127.0.0.1";
            const bool bStarted = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &m_process);
            Check(bStarted, "process: started a throwaway ping.exe");
            if (bStarted)
            {
                const std::string pid = std::to_string(m_process.dwProcessId);
                co_await ShowView("Processes");
                Check(Find("PID", pid) != nullptr, std::format("process: ping.exe (PID {}) is listed", pid));
                co_await Exec("Terminate Process", Find("PID", pid));
                Check(WaitForSingleObject(m_process.hProcess, 5000) == WAIT_OBJECT_0, "process: Terminate Process ended it");
                co_await ShowView("Processes");
                Check(Find("PID", pid) == nullptr, "process: it is gone from the view after the refresh");
            }
        }
        if (gone())
            co_return;

        // --- Startup Programs: Copy Command/Name, Disable, Enable, Delete on an HKCU Run entry
        {
            const std::wstring command = L"cmd.exe /c rem " + m_testNameW;
            if (RegistryValueExists(HKEY_CURRENT_USER, RunKey, m_testNameW) || RegistryValueExists(HKEY_CURRENT_USER, RunKey, L"Disabled_" + m_testNameW))
                Check(false, "startup: a Run value of the throwaway name already exists: section skipped");
            else
                m_bRunEntry = RegSetKeyValueW(HKEY_CURRENT_USER, RunKey, m_testNameW.c_str(), REG_SZ, command.c_str(), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
            Check(m_bRunEntry, std::format("startup: created the HKCU Run entry '{}'", m_testName));
            auto *s = static_cast<pserv::DataObject *>(nullptr);
            if (m_bRunEntry)
            {
                co_await ShowView("Startup Programs");
                s = Find("Name", m_testName);
                Check(s != nullptr, "startup: the entry is listed");
            }
            if (s)
            {
                const std::string enabledBefore = Property(s, "Enabled");
                co_await Exec("Copy Command", Find("Name", m_testName));
                Check(ReadClipboard() == pserv::utils::WideToUtf8(command), "startup: Copy Command");
                co_await Exec("Copy Name", Find("Name", m_testName));
                Check(ReadClipboard() == m_testName, "startup: Copy Name");
                // A refresh is requested right before the action, so one may be in flight while it
                // runs. No explicit refresh afterwards: the runner's own post-action refresh (and
                // the invalidation of the older one) must bring the new state.
                m_window.RequestRefresh(false);
                co_await Exec("Disable", Find("Name", m_testName));
                for (int i = 0; i < 100 && (m_window.IsRefreshInFlight() || m_window.Actions().IsBusy()); ++i)
                    co_await Pause(100);
                co_await Pause(300);
                const std::string enabledDisabled = Property(Find("Name", m_testName), "Enabled");
                Check(!enabledDisabled.empty() && enabledDisabled != enabledBefore,
                    std::format("startup: Disable changed Enabled ({} -> {}), seen without an explicit refresh", enabledBefore, enabledDisabled));
                co_await Exec("Enable", Find("Name", m_testName));
                co_await ShowView("Startup Programs");
                const std::string enabledAgain = Property(Find("Name", m_testName), "Enabled");
                Check(enabledAgain == enabledBefore, std::format("startup: Enable restored Enabled ({})", enabledAgain));
                co_await Exec("Delete", Find("Name", m_testName));
                co_await ShowView("Startup Programs");
                Check(Find("Name", m_testName) == nullptr, "startup: Delete removed it from the view");
                const bool bValueGone = !RegistryValueExists(HKEY_CURRENT_USER, RunKey, m_testNameW) && !RegistryValueExists(HKEY_CURRENT_USER, RunKey, L"Disabled_" + m_testNameW);
                Check(bValueGone, "startup: Delete removed the registry value");
                if (bValueGone)
                    m_bRunEntry = false;
            }
        }
        if (gone())
            co_return;

        // --- Scheduled Tasks: Copy Name/Path, Disable, Enable, Run Now, Delete on a throwaway task
        {
            DWORD rc = 1;
            if (RunProgram(std::format(L"schtasks.exe /query /tn {}", m_testNameW)) == 0)
                Check(false, "task: a task of the throwaway name already exists: section skipped");
            else
                rc = RunProgram(std::format(L"schtasks.exe /create /tn {} /tr \"cmd.exe /c rem\" /sc once /st 23:59", m_testNameW)); // no /f: never replace
            m_bTask = rc == 0;
            Check(m_bTask, std::format("task: schtasks created '{}' (exit {})", m_testName, rc));
            auto *t = static_cast<pserv::DataObject *>(nullptr);
            if (m_bTask)
            {
                co_await ShowView("Scheduled Tasks");
                t = Find("Name", m_testName);
                Check(t != nullptr, "task: it is listed");
            }
            if (t)
            {
                const std::string enabledBefore = Property(t, "Enabled");
                co_await Exec("Copy Name", Find("Name", m_testName));
                Check(ReadClipboard() == m_testName, "task: Copy Name");
                co_await Exec("Copy Path", Find("Name", m_testName));
                Check(ReadClipboard().find(m_testName) != std::string::npos, "task: Copy Path contains the name");
                co_await Exec("Disable", Find("Name", m_testName));
                co_await ShowView("Scheduled Tasks");
                const std::string enabledDisabled = Property(Find("Name", m_testName), "Enabled");
                Check(enabledDisabled != enabledBefore, std::format("task: Disable changed Enabled ({} -> {})", enabledBefore, enabledDisabled));
                co_await Exec("Enable", Find("Name", m_testName));
                co_await ShowView("Scheduled Tasks");
                Check(Property(Find("Name", m_testName), "Enabled") == enabledBefore, "task: Enable restored Enabled");
                co_await Exec("Run Now", Find("Name", m_testName));
                Check(m_lastMessages.empty(), "task: Run Now reported no error");
                co_await Exec("Delete", Find("Name", m_testName));
                co_await ShowView("Scheduled Tasks");
                const bool bGone = Find("Name", m_testName) == nullptr;
                Check(bGone, "task: Delete removed it");
                if (bGone)
                    m_bTask = false;
            }
        }
        if (gone())
            co_return;

        // --- Windows: Minimize, Restore, Maximize, Bring To Front, Hide, Show, Close on a window of our own
        {
            WNDCLASSEXW wc{sizeof(wc)};
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = WindowClass;
            RegisterClassExW(&wc);
            const std::string title = m_testName + "-window";
            m_testWindow = CreateWindowExW(0, WindowClass, pserv::utils::Utf8ToWide(title).c_str(), WS_OVERLAPPEDWINDOW, 40, 40, 320, 200, nullptr, nullptr, wc.hInstance, nullptr);
            ShowWindow(m_testWindow, SW_SHOWNOACTIVATE);
            Check(m_testWindow != nullptr, std::format("window: created '{}'", title));
            co_await ShowView("Windows");
            Check(Find("Title", title) != nullptr, "window: it is listed");
            co_await Exec("Minimize", Find("Title", title));
            Check(IsIconic(m_testWindow), "window: Minimize");
            co_await ShowView("Windows");
            co_await Exec("Restore", Find("Title", title));
            Check(!IsIconic(m_testWindow), "window: Restore");
            co_await ShowView("Windows");
            co_await Exec("Maximize", Find("Title", title));
            Check(IsZoomed(m_testWindow), "window: Maximize");
            co_await ShowView("Windows");
            co_await Exec("Restore", Find("Title", title));
            co_await ShowView("Windows");
            co_await Exec("Bring To Front", Find("Title", title));
            Check(m_lastMessages.empty(), "window: Bring To Front reported no error");
            co_await ShowView("Windows");
            co_await Exec("Hide", Find("Title", title));
            Check(!IsWindowVisible(m_testWindow), "window: Hide");
            co_await ShowView("Windows");
            if (auto *w = Find("Title", title))
            {
                co_await Exec("Show", w);
                Check(IsWindowVisible(m_testWindow), "window: Show");
            }
            else
            {
                Line("note  window: hidden windows are not listed, so Show was run after making it visible again");
                ShowWindow(m_testWindow, SW_SHOWNOACTIVATE);
            }
            co_await ShowView("Windows");
            co_await Exec("Close", Find("Title", title));
            co_await Pause(300);
            const bool bClosed = !IsWindow(m_testWindow);
            Check(bClosed, "window: Close destroyed it");
            if (bClosed)
                m_testWindow = nullptr;
        }
        if (gone())
            co_return;

        // --- Copy and export (the common actions), on the first service
        {
            co_await ShowView("Services");
            auto *c = m_window.CurrentController();
            auto *first = c && c->GetDataObjects().GetSize() ? *c->GetDataObjects().begin() : nullptr;
            const std::string name = first ? first->GetItemName() : std::string{};
            co_await Exec("Copy as JSON", first);
            const auto json = ReadClipboard();
            Check(!json.empty() && (json.front() == '[' || json.front() == '{') && json.find(name) != std::string::npos, "export: Copy as JSON");
            co_await ShowView("Services");
            first = Find("Name", name);
            co_await Exec("Copy as Plain Text", first);
            Check(ReadClipboard().find(name) != std::string::npos, "export: Copy as Plain Text");
            // Exports go into a directory this run creates exclusively. Sentinel files at the fixed
            // paths an earlier version used must survive untouched.
            std::map<std::filesystem::path, std::string> sentinels;
            for (const char *extension : {".json", ".txt"})
            {
                const auto path = std::filesystem::temp_directory_path() / std::format("pserv6-selftest-export{}", extension);
                std::error_code ec;
                if (!std::filesystem::exists(path, ec))
                {
                    std::ofstream{path} << "sentinel " << m_testName;
                    m_sentinels.push_back(path);
                }
                std::ifstream in{path};
                sentinels[path] = std::string{std::istreambuf_iterator<char>{in}, {}};
            }
            const auto dir = std::filesystem::temp_directory_path() / m_testName;
            if (CreateDirectoryW(dir.c_str(), nullptr))
                m_exportDir = dir;
            Check(!m_exportDir.empty(), std::format("export: created the run's own directory {}", dir.filename().string()));
            for (const auto &[action, extension] : {std::pair{"Export to JSON...", ".json"}, std::pair{"Export to Plain Text...", ".txt"}})
            {
                if (m_exportDir.empty())
                    break;
                const auto path = m_exportDir / std::format("export{}", extension);
                std::error_code ec;
                co_await ShowView("Services");
                co_await Exec(action, Find("Name", name), {{"output", pserv::utils::WideToUtf8(path.wstring())}});
                Check(std::filesystem::exists(path, ec) && std::filesystem::file_size(path, ec) > 0, std::format("export: {} wrote {}", action, path.filename().string()));
            }
            for (const auto &[path, content] : sentinels)
            {
                std::ifstream in{path};
                Check(std::string{std::istreambuf_iterator<char>{in}, {}} == content, std::format("export: {} at the old fixed path is untouched", path.filename().string()));
            }
        }
        if (gone())
            co_return;

        // --- Network Connections: the copy actions on the first connection
        {
            co_await ShowView("Network Connections");
            // A connection with a remote end (listening sockets have none: no "Copy Remote Endpoint")
            auto connected = [this]() -> pserv::DataObject *
            {
                for (auto *obj : m_window.CurrentController()->GetDataObjects())
                {
                    const auto remote = Property(obj, "Remote Port");
                    if (!remote.empty() && remote != "0" && remote != "*")
                        return obj;
                }
                return nullptr;
            };
            Check(connected() != nullptr, "network: a connection with a remote end is listed");
            for (const char *action : {"Copy Local Endpoint", "Copy Remote Endpoint", "Copy Process Name"})
            {
                co_await ShowView("Network Connections");
                const int executedBefore = m_executed;
                co_await Exec(action, connected());
                Check(m_executed > executedBefore && m_lastMessages.empty() && !ReadClipboard().empty(), std::format("network: {} ran and filled the clipboard", action));
            }
        }
        if (gone())
            co_return;

        // --- Remote and target-change guards (no second machine needed): a harmless recording
        //     action through the request path the menus use
        {
            static RecordingAction recorder{false}, localOnly{true};
            co_await ShowView("Services");
            auto *c = m_window.CurrentController();
            auto *row = c && c->GetDataObjects().GetSize() ? *c->GetDataObjects().begin() : nullptr;
            const std::string rowName = row ? row->GetItemName() : std::string{};
            const uint64_t generationBefore = m_window.TargetGeneration();

            // "Remote" = this computer by name: a real enumeration through the remote path
            wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1]{};
            DWORD computerLength = ARRAYSIZE(computer);
            GetComputerNameW(computer, &computerLength);
            const std::string machine = pserv::utils::WideToUtf8(computer);
            m_window.SetRemoteMachine(machine);
            Check(m_window.IsRemote(c), "guard: the Services controller counts as remote");
            for (int i = 0; i < 150 && c->GetPublishedTarget() != machine; ++i)
                co_await Pause(100);
            const bool bRemoteRows = c->GetPublishedTarget() == machine && c->GetDataObjects().GetSize() > 0;
            Check(bRemoteRows, std::format("guard: services enumerated through the remote path ({} rows from '{}')", c->GetDataObjects().GetSize(), c->GetPublishedTarget()));
            row = bRemoteRows ? *c->GetDataObjects().begin() : row;

            // An action that needs this machine is refused for remote rows...
            int before = localOnly.m_count;
            m_window.Actions().RequestAction(&localOnly, c, {row});
            m_lastMessages = m_window.Actions().TakeMessages();
            Check(localOnly.m_count == before && !m_lastMessages.empty(), "guard: a local-only action on remote rows is refused");
            // ...any other action runs, and is dispatched for the rows' machine
            before = recorder.m_count;
            m_window.Actions().RequestAction(&recorder, c, {row});
            for (int i = 0; i < 50 && m_window.Actions().IsBusy(); ++i)
                co_await Pause(100);
            m_window.Actions().TakeMessages();
            Check(recorder.m_count == before + 1 && recorder.m_lastTarget == machine,
                std::format("guard: an action on remote rows is dispatched for their machine ('{}')", recorder.m_lastTarget));

            // Back to this machine. Until the local rows are published, the rows on screen are
            // still the remote ones: the controller keeps counting as remote, and actions keep
            // their machine.
            for (int i = 0; i < 100 && (m_window.IsRefreshInFlight() || m_window.Actions().IsBusy()); ++i)
                co_await Pause(100);
            m_window.SetRemoteMachine({});
            if (bRemoteRows)
                Check(m_window.IsRemote(c) && c->GetPublishedTarget() == machine, "guard: still remote right after Connect to Local Machine (remote rows on screen)");
            co_await ShowView("Services");
            Check(c->GetPublishedTarget().empty(), "guard: local rows published after Connect to Local Machine");
            row = Find("Name", rowName);
            before = recorder.m_count;
            m_window.Actions().RequestAction(&recorder, m_window.CurrentController(), {row}, generationBefore);
            m_lastMessages = m_window.Actions().TakeMessages();
            Check(recorder.m_count == before && !m_lastMessages.empty(), "guard: a request from before the machine change is refused");

            // A current request on local rows runs
            before = recorder.m_count;
            m_window.Actions().RequestAction(&recorder, m_window.CurrentController(), {row});
            m_window.Actions().TakeMessages();
            Check(!m_window.IsRemote(m_window.CurrentController()) && recorder.m_count == before + 1, "guard: a current request on this machine's rows runs");
        }
        if (gone())
            co_return;

        // --- Services (administrator only): a throwaway service, Start (fails: not a service program), Uninstall
        if (IsElevated())
        {
            wil::unique_schandle scm{OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE)};
            wil::unique_schandle service{scm ? CreateServiceW(scm.get(), m_testNameW.c_str(), L"pserv6 self-test (throwaway)", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                                   SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, L"C:\\Windows\\System32\\cmd.exe /c rem", nullptr, nullptr, nullptr, nullptr, nullptr)
                                             : nullptr};
            // CreateServiceW fails (ERROR_SERVICE_EXISTS) rather than replace an existing service
            Check(service != nullptr, std::format("service: created the throwaway service '{}'", m_testName));
            if (service)
            {
                m_serviceName = m_testName;
                co_await ShowView("Services");
                co_await Exec("Start", Find("Name", m_testName));
                Line("      (cmd.exe is not a service program: Start is expected to report a failure)");
                co_await ShowView("Services");
                co_await Exec("Uninstall Service", Find("Name", m_testName));
                co_await ShowView("Services");
                const bool bGone = Find("Name", m_testName) == nullptr;
                Check(bGone, "service: Uninstall Service removed it");
                if (bGone)
                    m_serviceName.clear();
            }
        }
        else
        {
            Line("skip  services: Start, Stop, Restart, Pause, Resume, Uninstall Service, Delete Registry Key need administrator rights (run elevated)");
        }

        Line("not run (they only launch another program): Open in Registry Editor, Open in Explorer, Open Terminal Here, Open File Location, "
             "Open Containing Folder, Edit Configuration (task), Uninstall (installed programs); and Close Connection (needs administrator and a connection of its own)");
        Line(std::format("actions executed: {}, failed checks: {}", m_executed, m_failures));
        Cleanup();
        Line(m_failures == 0 ? "RESULT: PASS" : "RESULT: FAIL");
        m_window.Close();
    }

    void ActionTest::Cleanup()
    {
        // Whatever a failed step left behind
        if (!m_variableName.empty())
        {
            RegDeleteKeyValueW(HKEY_CURRENT_USER, L"Environment", pserv::utils::Utf8ToWide(m_variableName).c_str());
            m_variableName.clear();
        }
        if (m_process.hProcess && WaitForSingleObject(m_process.hProcess, 0) == WAIT_TIMEOUT)
            TerminateProcess(m_process.hProcess, 1);
        if (m_bRunEntry) // ours: created by this run (maybe disabled meanwhile)
        {
            RegDeleteKeyValueW(HKEY_CURRENT_USER, RunKey, m_testNameW.c_str());
            RegDeleteKeyValueW(HKEY_CURRENT_USER, RunKey, (L"Disabled_" + m_testNameW).c_str());
            RegDeleteKeyValueW(HKEY_CURRENT_USER, ApprovedRunKey, m_testNameW.c_str());
            m_bRunEntry = false;
        }
        if (m_bTask) // ours
        {
            RunProgram(std::format(L"schtasks.exe /delete /tn {} /f", m_testNameW));
            m_bTask = false;
        }
        if (m_testWindow && IsWindow(m_testWindow))
            DestroyWindow(m_testWindow);
        m_testWindow = nullptr;
        if (!m_serviceName.empty())
        {
            wil::unique_schandle scm{OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT)};
            wil::unique_schandle service{scm ? OpenServiceW(scm.get(), m_testNameW.c_str(), DELETE) : nullptr};
            if (service)
                DeleteService(service.get());
            m_serviceName.clear();
        }
        std::error_code ec;
        if (!m_exportDir.empty()) // ours: created exclusively by this run
            std::filesystem::remove_all(m_exportDir, ec);
        m_exportDir.clear();
        for (const auto &path : m_sentinels) // ours: created by this run
            std::filesystem::remove(path, ec);
        m_sentinels.clear();
    }
} // namespace pserv6
