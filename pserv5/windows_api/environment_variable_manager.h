#pragma once
#include <core/data_object.h>
#include <models/environment_variable_info.h>

namespace pserv
{

    class EnvironmentVariableManager
    {
    public:
        // Enumerate all environment variables (system and user)
        static std::vector<DataObject *> EnumerateEnvironmentVariables();

        // Modify environment variables
        static bool SetEnvironmentVariable(const std::string &name, const std::string &value, EnvironmentVariableScope scope);
        static bool DeleteEnvironmentVariable(const std::string &name, EnvironmentVariableScope scope);

    private:
        // Helper to enumerate from a specific registry key
        static void EnumerateFromKey(HKEY hKeyRoot, const std::wstring &subKeyPath, EnvironmentVariableScope scope, std::vector<DataObject *> &variables);

        // Helper to get registry key path for scope
        static std::wstring GetRegistryPath(EnvironmentVariableScope scope);
        static HKEY GetRegistryRoot(EnvironmentVariableScope scope);

        // Helper to notify system of environment changes
        static void BroadcastEnvironmentChange();
    };

} // namespace pserv
