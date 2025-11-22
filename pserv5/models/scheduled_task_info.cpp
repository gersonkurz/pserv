#include "precomp.h"
#include <models/scheduled_task_info.h>
#include <utils/string_utils.h>

namespace pserv
{

    ScheduledTaskInfo::ScheduledTaskInfo(std::string name,
        std::string path,
        std::string statusString,
        std::string trigger,
        std::string lastRunTime,
        std::string nextRunTime,
        std::string author,
        bool enabled,
        ScheduledTaskState state)
        : m_name(std::move(name))
        , m_path(std::move(path))
        , m_statusString(std::move(statusString))
        , m_trigger(std::move(trigger))
        , m_lastRunTime(std::move(lastRunTime))
        , m_nextRunTime(std::move(nextRunTime))
        , m_author(std::move(author))
        , m_bEnabled(enabled)
        , m_state(state)
    {
    }

    std::string ScheduledTaskInfo::GetProperty(int propertyId) const
    {
        switch (static_cast<ScheduledTaskProperty>(propertyId))
        {
        case ScheduledTaskProperty::Name:
            return m_name;
        case ScheduledTaskProperty::Status:
            return m_statusString;
        case ScheduledTaskProperty::Trigger:
            return m_trigger;
        case ScheduledTaskProperty::LastRun:
            return m_lastRunTime;
        case ScheduledTaskProperty::NextRun:
            return m_nextRunTime;
        case ScheduledTaskProperty::Author:
            return m_author;
        case ScheduledTaskProperty::Enabled:
            return GetEnabledString();
        default:
            return "";
        }
    }

    PropertyValue ScheduledTaskInfo::GetTypedProperty(int propertyId) const
    {
        switch (static_cast<ScheduledTaskProperty>(propertyId))
        {
        case ScheduledTaskProperty::Name:
            return PropertyValue{m_name};
        case ScheduledTaskProperty::Status:
            return PropertyValue{m_statusString};
        case ScheduledTaskProperty::Trigger:
            return PropertyValue{m_trigger};
        case ScheduledTaskProperty::LastRun:
            return PropertyValue{m_lastRunTime};
        case ScheduledTaskProperty::NextRun:
            return PropertyValue{m_nextRunTime};
        case ScheduledTaskProperty::Author:
            return PropertyValue{m_author};
        case ScheduledTaskProperty::Enabled:
            return PropertyValue{GetEnabledString()};
        default:
            return PropertyValue{std::string("")};
        }
    }

    bool ScheduledTaskInfo::MatchesFilter(const std::string &filter) const
    {
        if (filter.empty())
            return true;

        const auto lowerFilter = utils::ToLower(filter);

        return utils::ContainsIgnoreCase(m_name, lowerFilter) ||
               utils::ContainsIgnoreCase(m_statusString, lowerFilter) ||
               utils::ContainsIgnoreCase(m_trigger, lowerFilter) ||
               utils::ContainsIgnoreCase(m_author, lowerFilter);
    }

    std::string ScheduledTaskInfo::GetEnabledString() const
    {
        return m_bEnabled ? "Yes" : "No";
    }

} // namespace pserv
