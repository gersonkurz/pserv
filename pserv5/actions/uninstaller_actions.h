#pragma once

namespace pserv
{
    class DataAction;
    std::vector<const DataAction *> CreateUninstallerActions();

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> CreateAllUninstallerActions();
#endif

} // namespace pserv
