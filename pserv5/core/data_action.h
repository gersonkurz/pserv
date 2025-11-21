#pragma once

namespace pserv
{
    class DataObject;
    class DataController;
    class DataActionDispatchContext;

    // Defines where an action should be visible in the UI
    enum class ActionVisibility
    {
        ContextMenu = 1 << 0,      // Show in right-click context menu
        PropertiesDialog = 1 << 1, // Show as button in properties dialog
        Both = ContextMenu | PropertiesDialog
    };

    // Abstract base class for all data actions
    // Actions are self-contained objects that know how to execute themselves
    // and provide metadata about their visibility and availability
    class DataAction
    {
    private:
        const std::string m_name;
        const ActionVisibility m_visibility;

    protected:
        DataAction(std::string name, ActionVisibility visibility)
            : m_name{std::move(name)},
              m_visibility{visibility}
        {
        }

    public:
        virtual ~DataAction() = default;

        // Identity
        std::string GetName() const
        {
            return m_name;
        }

        // Visibility and availability
        ActionVisibility GetVisibility() const
        {
            return m_visibility;
        }
        virtual bool IsAvailableFor(const DataObject *object) const = 0;
        virtual bool IsSeparator() const
        {
            return false;
        }

        // Execution
        virtual void Execute(DataActionDispatchContext &context) const = 0;

        // UI hints
        virtual bool IsDestructive() const
        {
            return false;
        } // Red button color hint
        virtual bool RequiresConfirmation() const
        {
            return false;
        }
    };

    // Special action type representing a menu separator
    class DataActionSeparator final : public DataAction
    {
    public:
        DataActionSeparator()
            : DataAction{"", ActionVisibility::Both}
        {
        }
        bool IsAvailableFor(const DataObject *) const override
        {
            return true;
        }
        bool IsSeparator() const override
        {
            return true;
        }
        void Execute(DataActionDispatchContext &) const override
        {
        }
    };

    // ============================================================================
    // Properties Dialog Action
    // ============================================================================

    class DataPropertiesAction final : public DataAction
    {
    public:
        DataPropertiesAction()
            : DataAction{"Properties...", ActionVisibility::Both}
        {
        }

        bool IsAvailableFor(const DataObject *) const override
        {
            return true;
        }

        void Execute(DataActionDispatchContext &ctx) const override;
    };

    extern DataActionSeparator theDataActionSeparator;
    extern DataPropertiesAction theDataPropertiesAction;

} // namespace pserv
