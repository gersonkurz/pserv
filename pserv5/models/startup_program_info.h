#pragma once
#include <core/data_object.h>

namespace pserv
{

    enum class StartupProgramProperty
    {
        Name = 0,
        Command,
        Location,
        Type,
        Enabled
    };

    enum class StartupProgramType
    {
        RegistryRun,
        RegistryRunOnce,
        StartupFolder
    };

    enum class StartupProgramScope
    {
        System,  // HKLM or ProgramData
        User     // HKCU or AppData
    };

    class StartupProgramInfo : public DataObject
    {
    private:
        std::string m_name;
        std::string m_command;
        std::string m_location;          // Human-readable location description
        std::string m_registryPath;      // Full registry path (if registry-based)
        std::string m_registryValueName; // Registry value name (if registry-based)
        std::string m_filePath;          // Full file path (if file-based)
        StartupProgramType m_type;
        StartupProgramScope m_scope;
        bool m_bEnabled;

    public:
        StartupProgramInfo(std::string name,
            std::string command,
            std::string location,
            StartupProgramType type,
            StartupProgramScope scope,
            bool enabled);

        ~StartupProgramInfo() override = default;

        // DataObject interface
        static std::string GetStableID(const std::string &name, StartupProgramType type, StartupProgramScope scope)
        {
            return std::format("{}:{}:{}", name, static_cast<int>(type), static_cast<int>(scope));
        }
       
        std::string GetStableID() const
        {
            return GetStableID(m_name, m_type, m_scope);
        }

        std::string GetProperty(int propertyId) const override;
        PropertyValue GetTypedProperty(int propertyId) const override;
        bool MatchesFilter(const std::string &filter) const override;
        std::string GetItemName() const
        {
            return GetProperty(static_cast<int>(StartupProgramProperty::Name));
        }

        // Getters
        const std::string &GetName() const { return m_name; }
        const std::string &GetCommand() const { return m_command; }
        const std::string &GetLocation() const { return m_location; }
        const std::string &GetRegistryPath() const { return m_registryPath; }
        const std::string &GetRegistryValueName() const { return m_registryValueName; }
        const std::string &GetFilePath() const { return m_filePath; }
        StartupProgramType GetType() const { return m_type; }
        StartupProgramScope GetScope() const { return m_scope; }
        bool IsEnabled() const { return m_bEnabled; }

        std::string GetTypeString() const;
        std::string GetEnabledString() const;

        // Setters for registry/file paths
        void SetRegistryPath(std::string path) { m_registryPath = std::move(path); }
        void SetRegistryValueName(std::string valueName) { m_registryValueName = std::move(valueName); }
        void SetFilePath(std::string filePath) { m_filePath = std::move(filePath); }
        void SetEnabled(bool enabled) { m_bEnabled = enabled; }
    };

} // namespace pserv
