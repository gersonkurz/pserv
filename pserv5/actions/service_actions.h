#pragma once

namespace pserv
{
    class DataAction;
    // Factory function to create all service actions
    std::vector<const DataAction *> CreateServiceActions(DWORD currentState, DWORD controlsAccepted);

#ifdef PSERV_CONSOLE_BUILD
    // Console: Return ALL possible actions regardless of state (for command registration)
    std::vector<const DataAction *> CreateAllServiceActions();
#endif

} // namespace pserv
