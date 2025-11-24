#pragma once

namespace pserv
{
    class DataAction;
    std::vector<const DataAction *> CreateWindowActions();

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> CreateAllWindowActions();
#endif

} // namespace pserv
