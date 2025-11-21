#include "precomp.h"
#include <core/data_action.h>
#include <core/data_controller.h>
#include <dialogs/data_properties_dialog.h>

namespace pserv
{

    DataActionSeparator theDataActionSeparator;
    DataPropertiesAction theDataPropertiesAction;

    void DataPropertiesAction::Execute(DataActionDispatchContext &ctx) const
    {
        if (!ctx.m_selectedObjects.empty())
        {
            DataPropertiesDialog dialog{ctx.m_pController, ctx.m_selectedObjects};
            dialog.Open();
        }
    }
} // namespace pserv