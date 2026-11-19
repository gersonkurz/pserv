#pragma once
// pserv6 --selftest (plan section 7, R6): drives the real window through every view (or the
// listed ones) and writes a timing log. Command line:
//   pserv6.exe --selftest [View,View,...] [--seconds N] [--log <file>]
// Per view: switch -> first publish -> first rendered frame, then N seconds of continuous
// scrolling with a refresh every 2 s, recording frame intervals and the UI-thread work per
// publish. Ends with the working set, a PASS/FAIL line, and closes the window.

namespace winrt::pserv6::implementation
{
    struct MainWindow;
}

namespace pserv6
{
    class SelfTest final
    {
    public:
        /// nullptr unless the command line asks for a self-test.
        static std::unique_ptr<SelfTest> FromCommandLine(winrt::pserv6::implementation::MainWindow &window);

        SelfTest(winrt::pserv6::implementation::MainWindow &window, std::vector<std::string> views, int seconds, std::filesystem::path log);
        ~SelfTest();

        /// Called by the window after every publish of the current view, with the UI-thread time
        /// the publish task took (Publish + grid diff + realized update).
        void OnPublished(double uiMs);

    private:
        struct ViewResult
        {
            std::string name;
            size_t rows{0};
            double switchToPublishMs{-1};
            double switchToFrameMs{-1};
            double firstPublishUiMs{-1};    ///< The publish that populated the view.
            std::vector<double> publishUiMs; ///< Refresh publishes while measuring (at least one, or an error).
            std::vector<double> frameMs;
            std::string error;
        };

        void Start();
        void NextView();
        void OnRendering();
        void EndMeasure();
        void Finish();
        void Line(const std::string &text);

        winrt::pserv6::implementation::MainWindow &m_window;
        std::vector<std::string> m_views;
        int m_seconds;
        std::filesystem::path m_logPath;
        std::ofstream m_log;
        size_t m_index{0};
        enum class Stage
        {
            Idle,
            WaitPublish,
            WaitFrame,
            Measure,
            Done
        } m_stage{Stage::Idle};
        std::chrono::steady_clock::time_point m_switchAt{}, m_lastFrame{}, m_measureEnd{}, m_nextRefresh{};
        double m_scrollOffset{0};
        double m_scrollDirection{1};
        ViewResult m_current;
        std::vector<ViewResult> m_results;
        winrt::event_token m_renderingToken{};
        winrt::Microsoft::UI::Xaml::Controls::ScrollViewer m_scroller{nullptr};
        winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_timeout{nullptr};
    };
} // namespace pserv6
