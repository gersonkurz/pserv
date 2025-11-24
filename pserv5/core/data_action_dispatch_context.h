#pragma once

#ifdef PSERV_CONSOLE_BUILD
namespace argparse
{
    class ArgumentParser;
}
#endif

namespace pserv
{
    class DataController;
    class DataObject;
    class AsyncOperation;

    class DataActionDispatchContext final
    {
    public:
        DataActionDispatchContext() = default;
        ~DataActionDispatchContext();

        HWND m_hWnd{nullptr};
        AsyncOperation *m_pAsyncOp{nullptr};         // Current async operation
        std::vector<DataObject *> m_selectedObjects; // Selected services for multi-select
        DataController *m_pController{nullptr};      // Owning data controller
        bool m_bShowProgressDialog{false};
        bool m_bNeedsRefresh{false};
#ifdef PSERV_CONSOLE_BUILD
        argparse::ArgumentParser *m_pActionParser{nullptr}; // Console: action-specific argument parser
#endif
    };
} // namespace pserv
