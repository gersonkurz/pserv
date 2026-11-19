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

        /// @brief Validate an environment variable name.
        static bool IsValidVariableName(const std::string &name);

        /// @brief Check whether a variable exists in the specified scope (case-insensitive).
        static bool VariableExists(const std::string &name, EnvironmentVariableScope scope);

        /// @brief Create or update an environment variable.
        /// @param name Variable name.
        /// @param value Variable value.
        /// @param scope User or System scope.
        /// @param regType Registry value type (REG_SZ, REG_EXPAND_SZ, or 0 to preserve existing / auto-detect).
        /// @return true on success.
        static bool SetEnvironmentVariable(
            const std::string &name, const std::string &value, EnvironmentVariableScope scope, DWORD regType = 0);

        /// @brief Safely rename and update an environment variable without data loss or clobbering collisions.
        /// @param oldName Existing variable name.
        /// @param newName Replacement variable name.
        /// @param value Replacement variable value.
        /// @param scope User or System scope.
        /// @param preferredRegType Preferred registry type (0 to preserve existing).
        /// @return true on success.
        static bool RenameEnvironmentVariable(const std::string &oldName,
            const std::string &newName,
            const std::string &value,
            EnvironmentVariableScope scope,
            DWORD preferredRegType = 0);

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
