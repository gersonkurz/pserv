/// @file data_action.h
/// @brief Action system for operations on DataObjects.
///
/// Defines the DataAction base class and related types for implementing
/// context menu and properties dialog actions on data items.
///
/// Frontend contract: everything that needs the user happens in the frontend **before**
/// Execute() is called - confirmation (RequiresConfirmation / GetConfirmationText) and input
/// (GetParameters). Execute() never prompts; it reads the answers from the dispatch context and
/// reports problems through the context's IActionHost. This keeps the core free of any UI
/// toolkit, so one compiled core serves the console and WinUI frontends alike.
#pragma once

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

    /// @brief Kind of input an action parameter asks for.
    enum class ActionParameterKind
    {
        Text,         ///< Single-line text.
        SaveFilePath  ///< Path of a file to write; GUI frontends offer a save-file dialog.
    };

    /// @brief One file-type filter for a SaveFilePath parameter (e.g. {"JSON Files", "*.json"}).
    struct ActionFileFilter
    {
        std::string Name;
        std::string Pattern;
    };

    /// @brief Input an action needs from the user before it can execute.
    ///
    /// The frontend collects a value for every parameter and stores it in
    /// DataActionDispatchContext::m_parameters under Name. The console frontend maps each
    /// parameter to a `--<Name>` argument.
    struct ActionParameter
    {
        std::string Name;          ///< Key in the context, and the CLI option name (without "--").
        std::string Label;         ///< Prompt shown in GUI frontends.
        ActionParameterKind Kind{ActionParameterKind::Text};
        std::string DefaultValue;  ///< Pre-filled value (e.g. a suggested file name).
        std::vector<ActionFileFilter> FileFilters; ///< SaveFilePath only.
    };

    /// @brief Abstract base class for all data actions.
    ///
    /// Actions are self-contained command objects that encapsulate:
    /// - Identity: Name for display in menus/buttons
    /// - Visibility: Where the action appears in the UI
    /// - Availability: Whether the action applies to a given object
    /// - Input and confirmation: declared, collected by the frontend before Execute()
    /// - Execution: The actual operation to perform
    class DataAction
    {
    private:
        const std::string m_name;          ///< Display name for menus/buttons.
        const ActionVisibility m_visibility; ///< Where this action is shown.

    protected:
        /// @brief Construct an action with a name and visibility.
        DataAction(std::string name, ActionVisibility visibility)
            : m_name{std::move(name)},
              m_visibility{visibility}
        {
        }

        /// @brief Build a confirmation text: a headline, then one line per item, at most
        /// maxItems lines followed by "... and N more".
        static std::string FormatConfirmationList(std::string_view headline, const std::vector<std::string> &items, size_t maxItems = 10);

    public:
        virtual ~DataAction() = default;

        /// @brief Get the action's display name.
        std::string GetName() const { return m_name; }

        /// @brief Get the action's visibility flags.
        ActionVisibility GetVisibility() const { return m_visibility; }

        /// @brief Check if this action is available for a specific object.
        /// @param object The DataObject to check (may be nullptr for global actions).
        virtual bool IsAvailableFor(const DataObject *object) const = 0;

        /// @brief Check if this is a menu separator pseudo-action.
        virtual bool IsSeparator() const { return false; }

        /// @brief Execute the action. Never prompts: confirmation and parameters were collected
        /// by the frontend beforehand (see the file comment).
        virtual void Execute(DataActionDispatchContext &context) const = 0;

        /// @brief Check if this action is destructive (delete, stop, etc.); styled as a warning.
        virtual bool IsDestructive() const { return false; }

        /// @brief Check if the frontend must confirm with the user before Execute().
        /// The console frontend requires --force instead.
        virtual bool RequiresConfirmation() const { return false; }

        /// @brief The question to confirm, for the given selection. Only called when
        /// RequiresConfirmation() is true.
        virtual std::string GetConfirmationText(const std::vector<DataObject *> &selection) const
        {
            return std::format("Are you sure you want to run '{}' on {} item(s)?", m_name, selection.size());
        }

        /// @brief Input the frontend must collect before Execute(); empty by default.
        /// @param controller The controller the action runs on (for context-dependent defaults).
        virtual std::vector<ActionParameter> GetParameters(const DataController *controller) const { return {}; }
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

    /// @brief Built-in action to open the properties dialog for an object.
    ///
    /// Execute() asks the frontend's host to show its properties UI for the selection;
    /// frontends without one (the console) never list this action.
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

    /// @brief Global separator instance for reuse across controllers.
    extern DataActionSeparator theDataActionSeparator;

    /// @brief Global properties action instance for reuse across controllers.
    extern DataPropertiesAction theDataPropertiesAction;

} // namespace pserv
