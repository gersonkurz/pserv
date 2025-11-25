/// @file startup_program_info.h
/// @brief Data model for Windows startup program entries.
///
/// Contains StartupProgramInfo class representing a program configured
/// to run at Windows startup from registry Run keys or Startup folders.
#pragma once
#include <core/data_object.h>

namespace pserv
{
    /// @brief Column indices for startup program properties.
    enum class StartupProgramProperty
    {
        Name = 0,
        Command,
        Location,
        Type,
        Enabled
    };

    /// @brief Type of startup entry storage location.
    enum class StartupProgramType
    {
        RegistryRun,      ///< Registry Run key (persistent).
        RegistryRunOnce,  ///< Registry RunOnce key (one-time).
        StartupFolder     ///< Startup folder shortcut.
    };

    /// @brief Scope of the startup entry (user vs system-wide).
    enum class StartupProgramScope
    {
        System,  ///< HKLM or ProgramData (all users).
        User     ///< HKCU or AppData (current user only).
    };

    /// @brief Data model representing a startup program entry.
    ///
    /// Stores startup entry information from:
    /// - Registry Run/RunOnce keys (HKLM and HKCU)
    /// - Startup folders (common and per-user)
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
