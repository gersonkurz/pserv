#pragma once
#include <string>

namespace pserv {
    class DataObjectColumn {
    public:
        const std::string DisplayName;
        const std::string BindingName;

        DataObjectColumn(std::string displayName, std::string bindingName)
            : DisplayName(std::move(displayName)), BindingName(std::move(bindingName))
        {
        }
    };
}
