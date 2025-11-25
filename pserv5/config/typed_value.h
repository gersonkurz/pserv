/// @file typed_value.h
/// @brief Template class for storing typed configuration values.
///
/// TypedValue<T> represents a leaf node in the configuration tree that
/// holds an actual value of type T (int32_t, bool, or std::string).
#pragma once

#include <config/value_interface.h>

namespace pserv
{
    namespace config
    {

        extern std::shared_ptr<spdlog::logger> logger;

        /// @brief Template class for holding typed configuration values.
        /// @tparam T The value type: int32_t, bool, or std::string.
        ///
        /// TypedValue is a leaf node in the configuration tree. It stores a single
        /// value of type T along with a default value for reset functionality.
        ///
        /// @par Example Usage:
        /// @code
        /// TypedValue<int32_t> width{this, "Width", 1280};  // Default: 1280
        /// TypedValue<bool> maximized{this, "Maximized", false};
        /// TypedValue<std::string> theme{this, "Theme", "Dark"};
        ///
        /// // Reading
        /// int w = width.get();
        /// int w2 = width;  // Implicit conversion
        ///
        /// // Writing
        /// width.set(1920);
        /// @endcode
        template <typename T> class TypedValue : public ValueInterface
        {
        public:
            /// @brief Constructor for TypedValue.
            /// @param parentSection Every value must have a parent section, which is
            ///                      a Section that holds the configuration path.
            /// @param keyName Name of this value in the configuration path (the leaf name)
            /// @param defaultValue Default value for the underlying data
            TypedValue<T>(ValueInterface *parentSection, std::string keyName, T defaultValue = {})
                : m_keyName{std::move(keyName)},
                  m_parentSection{parentSection},
                  m_defaultValue{std::move(defaultValue)},
                  m_value{m_defaultValue}
            {
                if (logger)
                    logger->info("{}: creating TypedValue ({}, {}) at {}", __FUNCTION__, (const void *)parentSection, keyName, (const void *)this);
                if (parentSection)
                {
                    // Forward to a method in Section that knows about
                    // ValueInterface (Defined after Section is fully defined)
                    parentSection->addChildItem(this);
                }
            }

            ~TypedValue() override
            {
                logger->info("{}: destroying TypedValue ({}, {}) at {}", __FUNCTION__, (const void *)m_parentSection, m_keyName, (const void *)this);
            }

            /// @brief Accessor for the value
            /// @note Yes, I know, we also have operator overloading for const T &,
            ///       but this is more explicit and clear.
            /// @return Reference to the value stored in this TypedValue
            const T &get() const
            {
                return m_value;
            }

            /// @brief Assign a new value to this TypedValue.
            /// @note This will not save the value to the backend, you must call
            ///       `save()` on the ConfigBackend to persist the change.
            /// @param val new value to set
            void set(const T &val)
            {
                m_value = val;
            }

            /// @brief Implicit conversion operator to the stored type.
            /// @return A const reference to the stored value
            operator const T &() const
            {
                return m_value;
            }

            bool load(ConfigBackend &settings) override
            {
                return settings.load(getConfigPath(), m_value);
            }

            bool save(ConfigBackend &settings) const override
            {
                return settings.save(getConfigPath(), m_value);
            }

            /// @brief The path is actually relative to the parent section,
            ///        so we construct it by appending the key name to the parent's path.
            /// @return new path for this TypedValue
            std::string getConfigPath() const override
            {
                return m_parentSection->getConfigPath() + "/" + m_keyName;
            }

            void revertToDefault() override
            {
                m_value = m_defaultValue;
            }

            void addChildItem([[maybe_unused]] ValueInterface *item) override
            {
                // TypedValue does not support adding child items, so we ignore
                // this call. This is a no-op to satisfy the interface.
            }

        private:
            std::string m_keyName; // Leaf key name
            const ValueInterface *m_parentSection{nullptr};
            T m_defaultValue;
            T m_value;
        };
    } // namespace config
} // namespace pserv
