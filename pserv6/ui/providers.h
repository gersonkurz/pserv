#pragma once
#include <core/data_controller.h>

namespace pserv::winui
{
    struct Provider
    {
        const wchar_t* title;
        const wchar_t* glyph;
        std::shared_ptr<DataController> (*create)();
        std::vector<int> preferredColumns;
        std::vector<std::string> toolbarActions;
        bool supportsRemote{false};
        bool supportsAddVariable{false};
        bool labelsDisabledState{false};
    };
    const std::vector<Provider>& Providers();
}
