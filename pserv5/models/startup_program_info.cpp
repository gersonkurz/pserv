#include "precomp.h"
#include <models/startup_program_info.h>
#include <utils/string_utils.h>

namespace pserv
{

    StartupProgramInfo::StartupProgramInfo(std::string name,
        std::string command,
        std::string location,
        StartupProgramType type,
        StartupProgramScope scope,
        bool enabled)
        : m_name{std::move(name)},
          m_command{std::move(command)},
          m_location{std::move(location)},
          m_type{type},
          m_scope{scope},
          m_bEnabled{enabled}
    {
    }

    std::string StartupProgramInfo::GetProperty(int propertyId) const
    {
        switch (static_cast<StartupProgramProperty>(propertyId))
        {
        case StartupProgramProperty::Name:
            return m_name;
        case StartupProgramProperty::Command:
            return m_command;
        case StartupProgramProperty::Location:
            return m_location;
        case StartupProgramProperty::Type:
            return GetTypeString();
        case StartupProgramProperty::Enabled:
            return GetEnabledString();
        default:
            return "";
        }
    }

    PropertyValue StartupProgramInfo::GetTypedProperty(int propertyId) const
    {
        return PropertyValue{GetProperty(propertyId)};
    }

    bool StartupProgramInfo::MatchesFilter(const std::string &filter) const
    {
        if (filter.empty())
            return true;

        std::string lowerFilter = utils::ToLower(filter);
        return utils::ToLower(m_name).find(lowerFilter) != std::string::npos ||
               utils::ToLower(m_command).find(lowerFilter) != std::string::npos ||
               utils::ToLower(m_location).find(lowerFilter) != std::string::npos ||
               utils::ToLower(GetTypeString()).find(lowerFilter) != std::string::npos;
    }

    std::string StartupProgramInfo::GetTypeString() const
    {
        switch (m_type)
        {
        case StartupProgramType::RegistryRun:
            return "Registry (Run)";
        case StartupProgramType::RegistryRunOnce:
            return "Registry (RunOnce)";
        case StartupProgramType::StartupFolder:
            return "Startup Folder";
        default:
            return "Unknown";
        }
    }

    std::string StartupProgramInfo::GetEnabledString() const
    {
        return m_bEnabled ? "Yes" : "No";
    }

} // namespace pserv
