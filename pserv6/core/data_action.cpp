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
        if (!ctx.m_selectedObjects.empty() && ctx.m_pHost)
        {
            ctx.m_pHost->ShowProperties(ctx);
        }
    }

    std::string DataAction::FormatConfirmationList(std::string_view headline, const std::vector<std::string> &items, size_t maxItems)
    {
        std::string text{headline};
        text += "\n\n";
        for (size_t i = 0; i < items.size() && i < maxItems; ++i)
        {
            text += items[i];
            text += '\n';
        }
        if (items.size() > maxItems)
        {
            text += std::format("... and {} more\n", items.size() - maxItems);
        }
        return text;
    }

} // namespace pserv
