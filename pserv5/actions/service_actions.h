#pragma once
#include <core/data_action.h>

namespace pserv {

// Factory function to create all service actions
std::vector<const DataAction*> CreateServiceActions(DWORD currentState, DWORD controlsAccepted);

} // namespace pserv
