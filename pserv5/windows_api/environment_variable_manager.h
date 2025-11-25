/// @file environment_variable_manager.h
/// @brief Windows environment variable registry API wrapper.
///
/// Provides enumeration and modification of system and user environment
/// variables stored in the Windows registry.
#pragma once

#include <models/environment_variable_info.h>

namespace pserv
{
    class DataObjectContainer;

    /// @brief Static class for environment variable management.
    ///
    /// Reads and writes environment variables from registry:
    /// - User: HKCU\\Environment
    /// - System: HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment
    ///
    /// Broadcasts WM_SETTINGCHANGE after modifications so running
    /// applications can pick up the changes.
    class EnvironmentVariableManager final
    {
    public:
        /// @brief Enumerate all environment variables into a container.
        /// @param doc Container to populate with EnvironmentVariableInfo objects.
        static void EnumerateEnvironmentVariables(DataObjectContainer *doc);

        /// @brief Create or update an environment variable.
        /// @param name Variable name.
        /// @param value Variable value.
        /// @param scope User or System scope.
        /// @return true on success.
        static bool SetEnvironmentVariable(const std::string &name, const std::string &value, EnvironmentVariableScope scope);

        /// @brief Delete an environment variable.
        /// @param name Variable name.
        /// @param scope User or System scope.
        /// @return true on success.
        static bool DeleteEnvironmentVariable(const std::string &name, EnvironmentVariableScope scope);

    private:
        static void EnumerateFromKey(HKEY hKeyRoot, const std::wstring &subKeyPath, EnvironmentVariableScope scope, DataObjectContainer *doc);
        static std::wstring GetRegistryPath(EnvironmentVariableScope scope);
        static HKEY GetRegistryRoot(EnvironmentVariableScope scope);
        static void BroadcastEnvironmentChange();
    };

} // namespace pserv
