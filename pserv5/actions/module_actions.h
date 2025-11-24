#pragma once

namespace pserv
{
    class DataAction;
    std::vector<const DataAction *> CreateModuleActions();

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> CreateAllModuleActions();
#endif

} // namespace pserv
