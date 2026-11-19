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

} // namespace pserv