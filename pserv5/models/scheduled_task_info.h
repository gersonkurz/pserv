#pragma once
#include <core/data_object.h>

namespace pserv
{

    enum class ScheduledTaskProperty
    {
        Name = 0,
        Status,
        Trigger,
        LastRun,
        NextRun,
        Author,
        Enabled
    };

    enum class ScheduledTaskState
    {
        Unknown = 0,
        Disabled = 1,
        Queued = 2,
        Ready = 3,
        Running = 4
    };

    class ScheduledTaskInfo : public DataObject
    {
    private:
        std::string m_name;
        std::string m_path;           // Full task path (e.g., \Microsoft\Windows\Defrag\ScheduledDefrag)
        std::string m_statusString;   // Human-readable status
        std::string m_trigger;        // Trigger description
        std::string m_lastRunTime;    // Last run time formatted string
        std::string m_nextRunTime;    // Next run time formatted string
        std::string m_author;
        bool m_bEnabled;
        ScheduledTaskState m_state;

    public:
        ScheduledTaskInfo(std::string name,
            std::string path,
            std::string statusString,
            std::string trigger,
            std::string lastRunTime,
            std::string nextRunTime,
            std::string author,
            bool enabled,
            ScheduledTaskState state);
        ~ScheduledTaskInfo() override = default;

        // DataObject interface
        std::string GetProperty(int propertyId) const override;
        PropertyValue GetTypedProperty(int propertyId) const override;
        bool MatchesFilter(const std::string &filter) const override;
        std::string GetItemName() const
        {
            return GetProperty(static_cast<int>(ScheduledTaskProperty::Name));
        }

        // Getters
        const std::string &GetName() const { return m_name; }
        const std::string &GetPath() const { return m_path; }
        const std::string &GetStatusString() const { return m_statusString; }
        const std::string &GetTrigger() const { return m_trigger; }
        const std::string &GetLastRunTime() const { return m_lastRunTime; }
        const std::string &GetNextRunTime() const { return m_nextRunTime; }
        const std::string &GetAuthor() const { return m_author; }
        bool IsEnabled() const { return m_bEnabled; }
        ScheduledTaskState GetState() const { return m_state; }

        std::string GetEnabledString() const;
    };

} // namespace pserv
