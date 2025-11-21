#pragma once

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
    };
} // namespace pserv
