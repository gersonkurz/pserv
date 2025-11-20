#pragma once
#include <core/data_action.h>
#include <vector>
#include <memory>

namespace pserv {

// Factory function to create all service actions
std::vector<std::shared_ptr<DataAction>> CreateServiceActions();

} // namespace pserv
