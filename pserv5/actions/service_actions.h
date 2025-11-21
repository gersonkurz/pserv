#pragma once

namespace pserv
{
    class DataAction;
    // Factory function to create all service actions
    std::vector<const DataAction *> CreateServiceActions(DWORD currentState, DWORD controlsAccepted);

} // namespace pserv
