#pragma once
#include <core/data_object.h>

namespace pserv
{

    enum class EnvironmentVariableProperty
    {
        Name = 0,
        Value,
        Scope
    };

    enum class EnvironmentVariableScope
    {
        System,
        User
    };

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
