#pragma once
#include <string>

namespace pserv {
    class DataObjectColumn {
    public:
        std::string DisplayName;
        std::string BindingName;

        DataObjectColumn(std::string displayName, std::string bindingName)
            : DisplayName(std::move(displayName)), BindingName(std::move(bindingName))
        {
        }
    };
}
