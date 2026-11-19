#include "precomp.h"
#include <core/data_action_dispatch_context.h>
#include <core/async_operation.h>
#include <core/data_object.h>

namespace pserv
{
    DataActionDispatchContext::~DataActionDispatchContext()
    {
        // Release all selected objects
        for (auto *obj : m_selectedObjects)
        {
            obj->Release(REFCOUNT_DEBUG_ARGS);
        }
        m_selectedObjects.clear();

        if (m_pAsyncOp)
        {
            m_pAsyncOp->RequestCancel();
            m_pAsyncOp->Wait();
            delete m_pAsyncOp;
        }
    }

    std::string DataActionDispatchContext::GetParameter(const std::string &name) const
    {
        const auto it = m_parameters.find(name);
        return it != m_parameters.end() ? it->second : std::string{};
    }

    void DataActionDispatchContext::ShowMessage(ActionMessageKind kind, std::string_view title, std::string_view text)
    {
        switch (kind)
        {
        case ActionMessageKind::Error:
            spdlog::error("{}: {}", title, text);
            break;
        case ActionMessageKind::Warning:
            spdlog::warn("{}: {}", title, text);
            break;
        default:
            spdlog::info("{}: {}", title, text);
            break;
        }
        if (m_pHost)
        {
            m_pHost->ShowMessage(kind, title, text);
        }
    }

    void DataActionDispatchContext::SetSelection(const std::vector<DataObject *> &objects)
    {
        for (auto *obj : objects)
        {
            obj->Retain(REFCOUNT_DEBUG_ARGS);
        }
        for (auto *obj : m_selectedObjects)
        {
            obj->Release(REFCOUNT_DEBUG_ARGS);
        }
        m_selectedObjects = objects;
    }

} // namespace pserv
