#pragma once

namespace pserv
{
    class DataAction;
    std::vector<const DataAction *> CreateProcessActions();

#ifdef PSERV_CONSOLE_BUILD
    std::vector<const DataAction *> CreateAllProcessActions();
#endif

} // namespace pserv
