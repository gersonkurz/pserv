#include "precomp.h"
#include <core/data_action.h>
#include <core/data_controller.h>
#include <core/data_action_dispatch_context.h>

namespace pserv
{

    DataActionSeparator theDataActionSeparator;
    DataPropertiesAction theDataPropertiesAction;

    void DataPropertiesAction::Execute(DataActionDispatchContext &ctx) const
    {
        if (!ctx.m_selectedObjects.empty())
        {
            ctx.m_pController->ShowPropertiesDialog(ctx);
        }
    }
} // namespace pserv