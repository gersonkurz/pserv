/// @file environment_variable_info.h
/// @brief Data model for environment variable information.
///
/// Contains EnvironmentVariableInfo class representing a system or
/// user environment variable from the Windows registry.
#pragma once
#include <core/data_object.h>

namespace pserv
{
    /// @brief Column indices for environment variable properties.
    enum class EnvironmentVariableProperty
    {
        Name = 0,
        Value,
        Scope
    };

    /// @brief Scope of the environment variable.
    enum class EnvironmentVariableScope
    {
        System,  ///< System-wide variable (HKLM).
        User     ///< Per-user variable (HKCU).
    };

    /// @brief Data model representing an environment variable.
    ///
    /// Stores environment variable information from registry:
    /// - HKCU\\Environment for user variables
    /// - HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment for system variables
    class EnvironmentVariableInfo : public DataObject
    {
    private:
        std::string m_name;
        std::string m_value;
        EnvironmentVariableScope m_scope;

    public:
        EnvironmentVariableInfo(std::string name, std::string value, EnvironmentVariableScope scope);
        ~EnvironmentVariableInfo() override = default;

        // DataObject interface
        std::string GetProperty(int propertyId) const override;
        PropertyValue GetTypedProperty(int propertyId) const override;
        bool MatchesFilter(const std::string &filter) const override;
        std::string GetItemName() const
        {
            return GetProperty(static_cast<int>(EnvironmentVariableProperty::Name));
        }
        static std::string GetStableID(EnvironmentVariableScope scope, const std::string& name)
        {
            return std::format("{}:{}", static_cast<int>(scope), name);
        }

        std::string GetStableID() const
        {
            return GetStableID(m_scope, m_name);
        }
        // Getters
        const std::string &GetName() const
        {
            return m_name;
        }
        const std::string &GetValue() const
        {
            return m_value;
        }
        EnvironmentVariableScope GetScope() const
        {
            return m_scope;
        }
        std::string GetScopeString() const;

        // Setters (for editing)
        void SetName(std::string name)
        {
            m_name = std::move(name);
        }
        void SetValue(std::string value)
        {
            m_value = std::move(value);
        }
    };

} // namespace pserv
