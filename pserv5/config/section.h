/// @file section.h
/// @brief Container node for grouping configuration values.
///
/// Sections form the hierarchical structure of the configuration system.
/// They can contain TypedValue leaves and nested Section children.
#pragma once

#include <config/value_interface.h>

namespace pserv
{
    namespace config
    {

        /// @brief A container node in the configuration tree.
        ///
        /// Section groups related configuration values together and provides
        /// the hierarchical path structure. Sections can be nested to create
        /// arbitrarily deep configuration hierarchies.
        ///
        /// @par Example Usage:
        /// @code
        /// struct WindowSettings : public Section {
        ///     WindowSettings(Section* parent) : Section(parent, "Window") {}
        ///     TypedValue<int32_t> width{this, "Width", 1280};
        ///     TypedValue<int32_t> height{this, "Height", 720};
        /// };
        /// @endcode
        ///
        /// This creates a configuration path like "Window/Width" and "Window/Height".
        class Section : public ValueInterface
        {
        public:
            /// @brief Construct a root section (no parent).
            ///
            /// The root section has an empty path and serves as the top-level
            /// container for all configuration values.
            explicit Section();

            /// @brief Construct a child section.
            /// @param parent Parent section (or nullptr for standalone sections).
            /// @param groupNameAsKey Name of this section in the configuration path.
            Section(Section *parent, std::string groupNameAsKey);

            /// @brief Get the section's name (the last component of its path).
            /// @return Reference to the group name string.
            const std::string &getGroupName() const
            {
                return m_groupName;
            }

            bool load(ConfigBackend &settings) override;
            bool save(ConfigBackend &settings) const override;

            std::string getConfigPath() const override;

            void revertToDefault() override;

            /// @brief Get all child items (TypedValues and sub-Sections).
            /// @return Const reference to the vector of child pointers.
            const auto &getChildItems() const
            {
                return m_childItems;
            }

            void addChildItem(ValueInterface *item) override;

        private:
            Section *m_parent{nullptr};               ///< Parent section, or nullptr for root.
            std::string m_groupName;                  ///< This section's name in the path.
            std::vector<ValueInterface *> m_childItems; ///< Child values and sub-sections.
        };

    } // namespace config
} // namespace pserv
