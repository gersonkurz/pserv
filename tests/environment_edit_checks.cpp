#include "core/environment_edit.h"
#include <map>
#include <string>
#include <stdexcept>
#include <iostream>

struct Value { std::string text; uint32_t type; };
void Require(bool value) { if (!value) throw std::runtime_error("Environment edit regression failed"); }
int main()
{
    using namespace pserv;
    for (int scenario = 0; scenario < 6; ++scenario)
    {
        std::map<std::string, Value> values{{"old", {"%PATH%", 2}}};
        if (scenario == 1) values["new"] = {"existing", 1};
        auto result = EditEnvironmentValue(std::string("old"), std::string("new"), std::string("replacement"), false,
            [&](auto const& name, uint32_t& type)
            {
                if (scenario == 5) return EnvironmentRead::Failed;
                auto found = values.find(name);
                if (found == values.end()) return EnvironmentRead::Missing;
                type = found->second.type; return EnvironmentRead::Present;
            },
            [&](auto const& name, auto const& text, uint32_t type)
            {
                if (scenario == 2) return false;
                values[name] = {text, type}; return true;
            },
            [&](auto const& name)
            {
                if ((scenario == 3 || scenario == 4) && name == "old") return false;
                if (scenario == 4 && name == "new") return false;
                return values.erase(name) != 0;
            });
        if (scenario == 0) { Require(result == EnvironmentEditResult::Success); Require(!values.contains("old")); Require(values["new"].type == 2); }
        else
        {
            Require(result != EnvironmentEditResult::Success);
            Require(values["old"].text == "%PATH%" && values["old"].type == 2);
            if (scenario == 1) Require(values["new"].text == "existing");
            if (scenario == 2 || scenario == 3 || scenario == 5) Require(!values.contains("new"));
            if (scenario == 4) Require(result == EnvironmentEditResult::RollbackFailed && values["new"].text == "replacement");
        }
    }
    std::cout << "Environment edit checks passed: collision, write/delete/rollback/read failures, and expandable type preservation. No registry writes.\n";
}
