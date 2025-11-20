#pragma once
#include <string>
#include <memory>
#include <vector>

class DataObject;
class DataController;
struct DataActionDispatchContext;

// Defines where an action should be visible in the UI
enum class ActionVisibility {
    ContextMenu = 1 << 0,     // Show in right-click context menu
    PropertiesDialog = 1 << 1, // Show as button in properties dialog
    Both = ContextMenu | PropertiesDialog
};

// Abstract base class for all data actions
// Actions are self-contained objects that know how to execute themselves
// and provide metadata about their visibility and availability
class DataAction {
public:
    virtual ~DataAction() = default;

    // Identity
    virtual std::string GetName() const = 0;

    // Visibility and availability
    virtual ActionVisibility GetVisibility() const = 0;
    virtual bool IsAvailableFor(const DataObject* object) const = 0;
    virtual bool IsSeparator() const { return false; }

    // Execution
    virtual void Execute(const DataActionDispatchContext& context) = 0;

    // UI hints
    virtual bool IsDestructive() const { return false; }  // Red button color hint
    virtual bool RequiresConfirmation() const { return false; }
};

// Special action type representing a menu separator
class DataActionSeparator final : public DataAction {
public:
    std::string GetName() const override { return ""; }
    ActionVisibility GetVisibility() const override { return ActionVisibility::Both; }
    bool IsAvailableFor(const DataObject*) const override { return true; }
    bool IsSeparator() const override { return true; }
    void Execute(const DataActionDispatchContext&) override {}
};
