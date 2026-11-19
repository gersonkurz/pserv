#pragma once
// pserv6 --selftest-actions (plan Phase 5 exit): executes the views' actions against throwaway
// targets the test creates itself (a user environment variable, a child process, an HKCU Run
// entry, a scheduled task, a window of its own; with administrator rights also a service),
// through the same ActionRunner dispatch the context menu uses, without the dialogs. Every step
// checks its effect on the system. Destructive actions are never run against real objects; actions
// that only launch another program (Explorer, Registry Editor, a terminal, an uninstaller) are
// listed but not executed. Writes a log, ends with RESULT: PASS/FAIL, closes the window.

namespace winrt::pserv6::implementation
{
    struct MainWindow;
}

namespace pserv
{
    class DataController;
    class DataObject;
}

namespace pserv6
{
    class ActionTest final
    {
    public:
        /// nullptr unless the command line asks for it (--selftest-actions [--log <file>]).
        static std::unique_ptr<ActionTest> FromCommandLine(winrt::pserv6::implementation::MainWindow &window);

        ActionTest(winrt::pserv6::implementation::MainWindow &window, std::filesystem::path log);
        ~ActionTest();

    private:
        winrt::fire_and_forget Run();
        winrt::Windows::Foundation::IAsyncAction Pause(int milliseconds);
        winrt::Windows::Foundation::IAsyncAction ShowView(std::string name);
        winrt::Windows::Foundation::IAsyncAction Exec(std::string action, pserv::DataObject *pObject, std::map<std::string, std::string> parameters = {});
        pserv::DataObject *Find(std::string_view column, std::string_view value) const;
        std::string Property(pserv::DataObject *pObject, std::string_view column) const;
        void Check(bool bOk, const std::string &what);
        void Line(const std::string &text);
        void Cleanup();

        winrt::pserv6::implementation::MainWindow &m_window;
        std::ofstream m_log;
        std::filesystem::path m_logPath;
        std::shared_ptr<bool> m_alive{std::make_shared<bool>(true)};
        int m_failures{0};
        int m_executed{0};
        std::vector<std::string> m_lastMessages; ///< Recorded by the last Exec().
        bool m_bLastOffered{false};              ///< The last Exec() found the action.

        // Throwaway targets: unique per run (pserv6-selftest-<pid>-<ticks>), created only if
        // nothing of that name exists, and only those this run created are removed again.
        std::string m_testName;
        std::wstring m_testNameW;
        std::string m_variableName;
        wil::unique_process_information m_process;
        bool m_bRunEntry{false};
        bool m_bTask{false};
        HWND m_testWindow{nullptr};
        std::string m_serviceName;
        std::filesystem::path m_exportDir;                ///< Created by this run (exclusively), removed again.
        std::vector<std::filesystem::path> m_sentinels;   ///< Sentinel files this run created at the old fixed export paths.
    };
} // namespace pserv6
