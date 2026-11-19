#include "precomp.h"
#include <models/environment_variable_info.h>
#include <utils/string_utils.h>

namespace pserv
{

    EnvironmentVariableInfo::EnvironmentVariableInfo(std::string name, std::string value, EnvironmentVariableScope scope)
        : m_name{std::move(name)},
          m_value{std::move(value)},
          m_scope{scope}
    {
    }

    std::string EnvironmentVariableInfo::GetProperty(int propertyId) const
    {
        switch (static_cast<EnvironmentVariableProperty>(propertyId))
        {
        case EnvironmentVariableProperty::Name:
            return m_name;
        case EnvironmentVariableProperty::Value:
            return m_value;
        case EnvironmentVariableProperty::Scope:
            return GetScopeString();
        default:
            return "";
        }
    }
    PropertyValue EnvironmentVariableInfo::GetTypedProperty(int propertyId) const
    {
        return PropertyValue{GetProperty(propertyId)};
    }

    bool EnvironmentVariableInfo::MatchesFilter(const std::string &filter) const
    {
        if (filter.empty())
            return true;

        std::string lowerFilter = utils::ToLower(filter);
        return utils::ToLower(m_name).find(lowerFilter) != std::string::npos ||
               utils::ToLower(m_value).find(lowerFilter) != std::string::npos ||
               utils::ToLower(GetScopeString()).find(lowerFilter) != std::string::npos;
    }

    std::string EnvironmentVariableInfo::GetScopeString() const
    {
        switch (m_scope)
        {
        case EnvironmentVariableScope::System:
            return "System";
        case EnvironmentVariableScope::User:
            return "User";
        default:
            return "Unknown";
        }
    }

} // namespace pserv
