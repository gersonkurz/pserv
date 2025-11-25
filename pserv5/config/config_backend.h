/// @file config_backend.h
/// @brief Abstract interface for configuration storage backends.
///
/// This file defines the ConfigBackend abstract class which provides a
/// storage-agnostic interface for loading and saving configuration values.
/// Concrete implementations (e.g., TomlBackend) handle the actual file format.
#pragma once

namespace pserv
{
    namespace config
    {
        /// @brief Interface for enum values that can be serialized to/from strings.
        ///
        /// Implement this interface to allow enum types to be stored in configuration
        /// files as human-readable strings rather than raw integers.
        struct IEnumConfigValue
        {
            virtual ~IEnumConfigValue() = default;

            /// @brief Convert the enum value to its string representation.
            /// @return String representation of the current enum value.
            virtual std::string toString() const = 0;

            /// @brief Parse a string and set the enum value.
            /// @param str String to parse.
            /// @return true if parsing succeeded, false otherwise.
            virtual bool fromString(std::string_view str) = 0;
        };

        /// @brief Abstract base class for configuration storage backends.
        ///
        /// ConfigBackend defines the interface for reading and writing configuration
        /// values to persistent storage. The path format uses forward slashes as
        /// separators (e.g., "Window/Width" or "Application/Theme").
        ///
        /// Implementations must be non-copyable and non-movable to prevent
        /// accidental duplication of file handles or cached state.
        class ConfigBackend
        {
        public:
            ConfigBackend() = default;
            virtual ~ConfigBackend() = default;

            // Non-copyable and non-movable
            ConfigBackend(const ConfigBackend &) = delete;
            ConfigBackend &operator=(const ConfigBackend &) = delete;
            ConfigBackend(ConfigBackend &&) = delete;
            ConfigBackend &operator=(ConfigBackend &&) = delete;

            /// @name Type-specific load/save methods
            /// @{

            /// @brief Load a 32-bit integer value from the configuration.
            /// @param path Configuration path (e.g., "Window/Width").
            /// @param value Output parameter for the loaded value.
            /// @return true if the value was loaded successfully, false otherwise.
            virtual bool load(const std::string &path, int32_t &value) = 0;

            /// @brief Save a 32-bit integer value to the configuration.
            /// @param path Configuration path (e.g., "Window/Width").
            /// @param value Value to save.
            /// @return true if the value was saved successfully, false otherwise.
            virtual bool save(const std::string &path, int32_t value) = 0;

            /// @brief Load a boolean value from the configuration.
            /// @param path Configuration path.
            /// @param value Output parameter for the loaded value.
            /// @return true if the value was loaded successfully, false otherwise.
            virtual bool load(const std::string &path, bool &value) = 0;

            /// @brief Save a boolean value to the configuration.
            /// @param path Configuration path.
            /// @param value Value to save.
            /// @return true if the value was saved successfully, false otherwise.
            virtual bool save(const std::string &path, bool value) = 0;

            /// @brief Load a string value from the configuration.
            /// @param path Configuration path.
            /// @param value Output parameter for the loaded value.
            /// @return true if the value was loaded successfully, false otherwise.
            virtual bool load(const std::string &path, std::string &value) = 0;

            /// @brief Save a string value to the configuration.
            /// @param path Configuration path.
            /// @param value Value to save.
            /// @return true if the value was saved successfully, false otherwise.
            virtual bool save(const std::string &path, const std::string &value) = 0;

            /// @}

            /// @brief Load an enum value from the configuration.
            /// @param path Configuration path.
            /// @param value Enum value implementing IEnumConfigValue.
            /// @return true if the value was loaded and parsed successfully.
            bool load(const std::string &path, IEnumConfigValue &value)
            {
                std::string str;
                if (load(path, str))
                {
                    return value.fromString(str);
                }
                return false;
            }

            /// @brief Save an enum value to the configuration.
            /// @param path Configuration path.
            /// @param value Enum value implementing IEnumConfigValue.
            /// @return true if the value was saved successfully.
            bool save(const std::string &path, const IEnumConfigValue &value)
            {
                std::string str = value.toString();
                return save(path, str);
            }

            /// @brief Check if a section exists in the configuration.
            /// @param path Path to the section (e.g., "Window").
            /// @return true if the section exists, false otherwise.
            virtual bool sectionExists(const std::string &path) = 0;

            /// @brief Delete a single key from the configuration.
            /// @param path Full path to the key (e.g., "Window/Width").
            /// @return true if the key was deleted, false if it didn't exist.
            virtual bool deleteKey(const std::string &path) = 0;

            /// @brief Delete an entire section and all its contents.
            /// @param path Path to the section (e.g., "Window").
            /// @return true if the section was deleted, false if it didn't exist.
            virtual bool deleteSection(const std::string &path) = 0;
        };
    } // namespace config
} // namespace pserv
