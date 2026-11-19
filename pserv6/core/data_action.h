/// @file data_action.h
/// @brief Action system for operations on DataObjects.
///
/// Defines the DataAction base class and related types for implementing
/// context menu and properties dialog actions on data items.
#pragma once

#ifdef PSERV_CONSOLE_BUILD
namespace argparse
{
    class ArgumentParser;
}
#endif

namespace pserv
{
    class DataObject;
    class DataController;
    class DataActionDispatchContext;

    /// @brief Flags controlling where an action appears in the UI.
    ///
    /// Actions can be shown in context menus, properties dialogs, or both.
    /// Use bitwise OR to combine flags.
    enum class ActionVisibility
    {
        ContextMenu = 1 << 0,      ///< Show in right-click context menu.
        PropertiesDialog = 1 << 1, ///< Show as button in properties dialog.
        Both = ContextMenu | PropertiesDialog ///< Show in both locations.
    };

    /// @brief Abstract base class for all data actions.
    ///
    /// Actions are self-contained command objects that encapsulate:
    /// - Identity: Name for display in menus/buttons
    /// - Visibility: Where the action appears in the UI
    /// - Availability: Whether the action applies to a given object
    /// - Execution: The actual operation to perform
    /// - UI hints: Styling and confirmation requirements
    ///
    /// @par Implementing a Custom Action:
    /// @code
    /// class MyAction : public DataAction {
    /// public:
    ///     MyAction() : DataAction{"Do Thing", ActionVisibility::Both} {}
    ///     bool IsAvailableFor(const DataObject* obj) const override {
    ///         return obj != nullptr;  // Available for any object
    ///     }
    ///     void Execute(DataActionDispatchContext& ctx) const override {
    ///         // Perform the action
    ///     }
    /// };
    /// @endcode
    class DataAction
    {
    private:
        const std::string m_name;          ///< Display name for menus/buttons.
        const ActionVisibility m_visibility; ///< Where this action is shown.

    protected:
        /// @brief Construct an action with a name and visibility.
        /// @param name Display name shown in menus and buttons.
        /// @param visibility Where the action should appear in the UI.
        DataAction(std::string name, ActionVisibility visibility)
            : m_name{std::move(name)},
              m_visibility{visibility}
        {
        }

    public:
        virtual ~DataAction() = default;

        /// @brief Get the action's display name.
        std::string GetName() const { return m_name; }

        /// @brief Get the action's visibility flags.
        ActionVisibility GetVisibility() const { return m_visibility; }

        /// @brief Check if this action is available for a specific object.
        /// @param object The DataObject to check (may be nullptr for global actions).
        /// @return true if the action can be executed on this object.
        virtual bool IsAvailableFor(const DataObject *object) const = 0;

        /// @brief Check if this is a menu separator pseudo-action.
        virtual bool IsSeparator() const { return false; }

        /// @brief Execute the action.
        /// @param context Provides access to selected objects, controller, and UI handles.
        virtual void Execute(DataActionDispatchContext &context) const = 0;

#ifdef PSERV_CONSOLE_BUILD
        /// @brief Register custom command-line arguments for this action.
        /// @param cmd The ArgumentParser for this action's subcommand.
        /// Override to add action-specific CLI parameters (e.g., --output for export).
        virtual void RegisterArguments(argparse::ArgumentParser &cmd) const
        {
            // Default: no custom arguments
        }
#endif

        /// @brief Check if this action is destructive (delete, stop, etc.).
        /// @return true to render button with warning color.
        virtual bool IsDestructive() const { return false; }

        /// @brief Check if this action requires user confirmation.
        /// @return true to show a confirmation dialog before execution.
        virtual bool RequiresConfirmation() const { return false; }
    };

    /// @brief Pseudo-action representing a visual separator in menus.
    ///
    /// Insert this action between groups of related actions to add
    /// a horizontal separator line in the context menu.
    class DataActionSeparator final : public DataAction
    {
    public:
        DataActionSeparator()
            : DataAction{"", ActionVisibility::Both}
        {
        }
        bool IsAvailableFor(const DataObject *) const override { return true; }
        bool IsSeparator() const override { return true; }
        void Execute(DataActionDispatchContext &) const override { }
    };

#ifndef PSERV_CONSOLE_BUILD
    /// @brief Built-in action to open the properties dialog for an object.
    ///
    /// This action is typically added as the last item in context menus
    /// and triggers the properties dialog for viewing/editing object details.
    class DataPropertiesAction final : public DataAction
    {
    public:
        DataPropertiesAction()
            : DataAction{"Properties...", ActionVisibility::Both}
        {
        }

        bool IsAvailableFor(const DataObject *) const override { return true; }
        void Execute(DataActionDispatchContext &ctx) const override;
    };
#endif

    /// @brief Global separator instance for reuse across controllers.
    extern DataActionSeparator theDataActionSeparator;

#ifndef PSERV_CONSOLE_BUILD
    /// @brief Global properties action instance for reuse across controllers.
    extern DataPropertiesAction theDataPropertiesAction;
#endif

} // namespace pserv
