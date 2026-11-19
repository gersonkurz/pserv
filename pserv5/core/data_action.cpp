#include "precomp.h"
#include <core/data_action.h>
#include <core/data_controller.h>
#include <core/data_action_dispatch_context.h>

namespace pserv
{

    DataActionSeparator theDataActionSeparator;
#if !defined(PSERV_CONSOLE_BUILD) && !defined(PSERV_WINUI_BUILD)
    DataPropertiesAction theDataPropertiesAction;

    void DataPropertiesAction::Execute(DataActionDispatchContext &ctx) const
    {
        if (!ctx.m_selectedObjects.empty())
        {
            ctx.m_pController->ShowPropertiesDialog(ctx);
        }
    }
#endif

} // namespace pserv