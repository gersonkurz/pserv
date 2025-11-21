#include "precomp.h"
#include <core/data_action_dispatch_context.h>
#include <core/async_operation.h>

namespace pserv
{
    DataActionDispatchContext::~DataActionDispatchContext()
    {
        if (m_pAsyncOp)
        {
            m_pAsyncOp->RequestCancel();
            m_pAsyncOp->Wait();
            delete m_pAsyncOp;
        }
    }

} // namespace pserv