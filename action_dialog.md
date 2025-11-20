# Action and Properties Dialog Redesign

## Overview

This document outlines the redesign of the action system and properties dialogs to eliminate duplication while preserving necessary specialization. The goal is to make actions first-class abstract objects (like DataObjects and Columns) and create a unified properties dialog framework.

## Design Principles

1. **Actions as Objects**: Actions should be self-contained objects that know how to execute themselves and create their UI elements
2. **Single Properties Dialog**: One common dialog framework that works across all controllers
3. **Preserve Specialization**: Keep domain-specific logic (action behavior, dialog content) in controllers
4. **Eliminate Duplication**: Remove boilerplate for dialog lifecycle, tab management, action dispatch patterns

## Design Decisions

- **Action UI**: Actions create buttons only (no complex UI widgets for now)
- **Button Layout**: Action buttons appear contextually within each tab
- **Edit Support**: Editing remains controller-specific; dialog provides framework but doesn't enforce edit patterns
- **Action Scope**: Same action objects appear in both context menus and properties dialogs

---

## CRITICAL WORKFLOW RULE

**⚠️ MANDATORY PROCESS FOR EACH STEP ⚠️**

After completing the code for each step:

1. **STOP CODING** - Do not proceed to the next step
2. **INFORM THE HUMAN** - Report what was completed
3. **HUMAN COMPILES** - Wait for human to build the project manually (MSBuild CLI issues)
4. **HUMAN TESTS** - Wait for human to test the functionality
5. **HUMAN INSPECTS CODE** - Wait for human to review the implementation
6. **WAIT FOR EXPLICIT PERMISSION** - Only proceed when human explicitly says "continue" or "next step"

Do NOT:
- Batch multiple steps together
- Assume approval to continue
- Skip the human verification cycle
- Attempt to build the project yourself

This incremental, checkpoint-based approach ensures we catch issues early and maintain code quality throughout the refactor.

---

## DOCUMENT MAINTENANCE RULE

**⚠️ KEEPING THE PLAN CURRENT ⚠️**

After each step is completed AND human has verified it:

1. **REMOVE THE COMPLETED STEP** from this document
2. **ADD A BRIEF SUMMARY** to the "Completed Steps" section at the top
3. **UPDATE THE FILE** immediately after human verification but BEFORE committing code
4. Keep only upcoming steps in the detailed sections

**Summary Format**:
```
✅ Step X.Y: Brief description - COMPLETED (date)
   - What was done
   - What was verified
```

This ensures:
- The plan always shows current progress
- Easy to resume after session interruptions
- Document stays manageable in size
- Clear history of what's been accomplished

---

## COMPLETED STEPS

*(This section will be populated as steps are completed)*

---

---

## Milestone Verification Points

Each milestone represents a stable, testable state where the system should fully function.

### Milestone M1: Action Abstraction Foundation Complete
**After**: Phase 1 complete
**Verification**:
- [ ] Project compiles without errors
- [ ] All action classes instantiate correctly
- [ ] ServicesDataController can create action objects
- [ ] GetActions() returns valid action list
- [ ] Backward compatibility shims work (old enum-based menus still function)
- [ ] Context menus appear and work identically to before

**Test**: Right-click on service → verify menu appears with all actions → execute Start/Stop → verify they work

---

### Milestone M2: All Controllers Use Action Objects
**After**: Phase 2 complete
**Verification**:
- [ ] Project compiles without errors
- [ ] All 5 controllers have action registries
- [ ] All controllers implement GetActions()
- [ ] All action Execute() methods contain moved code
- [ ] Old and new interfaces coexist
- [ ] Context menus work for all controller types (services, processes, windows, uninstaller, modules)

**Test**: Test context menu actions in each tab (Services, Processes, Windows, Uninstaller, Modules) → verify all actions execute correctly

---

### Milestone M3: Common Properties Dialog Framework Ready
**After**: Phase 3 complete
**Verification**:
- [ ] Project compiles without errors
- [ ] CommonPropertiesDialog class compiles and links
- [ ] Dialog can be instantiated
- [ ] Dialog lifecycle (Open/Close/Render) works
- [ ] Action button rendering logic is correct
- [ ] Tab management works for multi-select

**Test**: Create minimal test integration in one controller → open dialog → verify tabs render → verify action buttons appear

---

### Milestone M4: All Controllers Use Common Dialog
**After**: Phase 4 complete
**Verification**:
- [ ] Project compiles without errors
- [ ] All controllers migrated to CommonPropertiesDialog
- [ ] Properties dialogs open from context menu
- [ ] All property fields render correctly
- [ ] Action buttons appear in dialogs
- [ ] Tabs work for multi-select
- [ ] Service editing still works (if applicable)
- [ ] Module properties dialog now exists and works
- [ ] Old dialog code still present but unused

**Test**: Open properties for each controller type → verify content renders → verify action buttons work → test service editing → test modules for first time

---

### Milestone M5: MainWindow Uses Action Objects
**After**: Phase 5 complete
**Verification**:
- [ ] Project compiles without errors
- [ ] Context menus built from action objects
- [ ] Old enum-based code path removed from MainWindow
- [ ] Action filtering by visibility works
- [ ] Action availability checks work
- [ ] Multi-select label "(X selected)" appears
- [ ] Async operations still work

**Test**: Full regression test of all actions from context menus → verify async operations complete → verify progress dialogs appear

---

### Milestone M6: Legacy Code Removed
**After**: Phase 6 complete
**Verification**:
- [ ] Project compiles without errors
- [ ] Old dialog files deleted
- [ ] Old action enums deleted
- [ ] Old action methods deleted
- [ ] CMakeLists.txt updated
- [ ] No references to deleted code remain
- [ ] All functionality still works

**Test**: Full regression test across all controllers → verify nothing broke

---

### Milestone M7: Production Ready
**After**: Phases 7-8 complete
**Verification**:
- [ ] All test cases pass
- [ ] No code duplication remains
- [ ] No memory leaks detected
- [ ] Code review complete
- [ ] Comments added
- [ ] TODOs updated
- [ ] Architecture documented

**Test**: Comprehensive testing of all scenarios, edge cases, and error conditions

---

## Phase 1: Create Action Abstraction

**Milestone Target**: M1 - Action Abstraction Foundation Complete

### Step 1.1: Create data_action.h header file

**File**: `pserv5/core/data_action.h` (NEW)

**Purpose**: Define abstract base class for actions.

**Tasks**:
1. Create the new file `pserv5/core/data_action.h`
2. Add include guards and necessary includes
3. Define `ActionVisibility` enum
4. Define `DataAction` abstract base class with all virtual methods
5. Define `DataActionSeparator` concrete class

**Implementation**:
```cpp
#pragma once
#include <string>
#include <memory>
#include <vector>

class DataObject;
class DataController;
struct DataActionDispatchContext;

enum class ActionVisibility {
    ContextMenu = 1 << 0,     // Show in right-click context menu
    PropertiesDialog = 1 << 1, // Show as button in properties dialog
    Both = ContextMenu | PropertiesDialog
};

class DataAction {
public:
    virtual ~DataAction() = default;

    // Identity
    virtual int32_t GetId() const = 0;
    virtual std::string GetName() const = 0;

    // Visibility and availability
    virtual ActionVisibility GetVisibility() const = 0;
    virtual bool IsAvailableFor(const DataObject* object) const = 0;
    virtual bool IsSeparator() const { return false; }

    // Execution
    virtual void Execute(const DataActionDispatchContext& context) = 0;

    // UI hints
    virtual bool IsDestructive() const { return false; }  // Red button color
    virtual bool RequiresConfirmation() const { return false; }
};

// Separator is a special action type
class DataActionSeparator final : public DataAction {
public:
    int32_t GetId() const override { return -1; }
    std::string GetName() const override { return ""; }
    ActionVisibility GetVisibility() const override { return ActionVisibility::Both; }
    bool IsAvailableFor(const DataObject*) const override { return true; }
    bool IsSeparator() const override { return true; }
    void Execute(const DataActionDispatchContext&) override {}
};
```

**Key Design Points**:
- `GetId()` returns int32_t to match existing enum-based action IDs
- `IsAvailableFor()` allows per-object availability checks (e.g., "Start" only available for stopped services)
- `GetVisibility()` controls where action appears (context menu, properties dialog, or both)
- `IsDestructive()` provides UI hint for button coloring
- `IsSeparator()` special case for menu separators

**🛑 STOP - CHECKPOINT 1.1**
- Human compiles project
- Verify: File compiles without errors
- Human reviews code
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 1.2: Review DataActionDispatchContext

**File**: `pserv5/core/data_controller.h`

**Purpose**: Ensure dispatch context is sufficient for action execution.

**Tasks**:
1. Open `pserv5/core/data_controller.h`
2. Locate `DataActionDispatchContext` struct
3. Verify it contains all necessary fields
4. Add comments if needed to document usage by actions

**No code changes expected** - just verification.

**🛑 STOP - CHECKPOINT 1.2**
- Human compiles project (should still compile)
- Verify: Context has all necessary fields
- Human reviews documentation
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 1.3: Create service_actions.h with stub declarations

**File**: `pserv5/actions/service_actions.h` (NEW)

**Purpose**: Declare all service action classes (implementations come later).

**Tasks**:
1. Create directory `pserv5/actions/` if it doesn't exist
2. Create file `pserv5/actions/service_actions.h`
3. Add include for `data_action.h`
4. Declare ALL 17 service action classes (just headers, no implementations yet)

**Classes to Declare**:
- ServiceStartAction
- ServiceStopAction
- ServiceRestartAction
- ServicePauseAction
- ServiceResumeAction
- ServiceSetStartupAutoAction
- ServiceSetStartupManualAction
- ServiceSetStartupDisabledAction
- ServiceCopyNameAction
- ServiceCopyDisplayNameAction
- ServiceCopyBinaryPathAction
- ServiceOpenInRegistryAction
- ServiceOpenInExplorerAction
- ServiceOpenTerminalAction
- ServiceUninstallAction
- ServiceDeleteRegistryKeyAction
- ServicePropertiesAction

**Pattern for each**:
```cpp
class ServiceStartAction final : public DataAction {
public:
    int32_t GetId() const override;
    std::string GetName() const override;
    ActionVisibility GetVisibility() const override;
    bool IsAvailableFor(const DataObject* object) const override;
    void Execute(const DataActionDispatchContext& context) override;
};
```

**🛑 STOP - CHECKPOINT 1.3**
- Human compiles project
- Verify: All action classes declared, compiles without errors
- Human reviews class declarations
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 1.4: Implement service_actions.cpp - Part A (Identity methods)

**File**: `pserv5/actions/service_actions.cpp` (NEW)

**Purpose**: Implement GetId() and GetName() for all service actions.

**Tasks**:
1. Create `pserv5/actions/service_actions.cpp`
2. Include necessary headers (service_actions.h, service_info.h, existing ServiceAction enum)
3. Implement GetId() and GetName() for ALL 17 actions
4. Leave GetVisibility(), IsAvailableFor(), and Execute() as stubs returning safe defaults

**Example stub Execute()**:
```cpp
void ServiceStartAction::Execute(const DataActionDispatchContext& context) {
    // TODO: Implementation in step 2.3
}
```

**🛑 STOP - CHECKPOINT 1.4**
- Human compiles project
- Verify: Links successfully, all GetId/GetName implemented
- Human reviews naming and IDs
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 1.5: Implement service_actions.cpp - Part B (Visibility and Availability)

**Purpose**: Complete GetVisibility() and IsAvailableFor() for all service actions.

**Tasks**:
1. Implement GetVisibility() for each action (refer to existing context menu logic)
2. Implement IsAvailableFor() for each action (move logic from existing GetAvailableActions())
3. Keep Execute() as stubs

**Examples**:
- Start: Available only when service is stopped
- Stop: Available only when service is running
- Properties: Always available, visible in Both
- SetStartup*: Always available, visible in ContextMenu only

**🛑 STOP - CHECKPOINT 1.5**
- Human compiles project
- Verify: Compiles and links
- Human reviews availability logic
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 1.6: Create action files for other controllers (declarations only)

**Files to Create**:
- `pserv5/actions/process_actions.h/.cpp` (9 actions)
- `pserv5/actions/window_actions.h/.cpp` (8 actions)
- `pserv5/actions/uninstaller_actions.h/.cpp` (2 actions)
- `pserv5/actions/module_actions.h/.cpp` (2 actions)

**Tasks for EACH file**:
1. Create .h with class declarations (pattern from Step 1.3)
2. Create .cpp with GetId/GetName implementations
3. Leave GetVisibility/IsAvailableFor/Execute as stubs

**🛑 STOP - CHECKPOINT 1.6**
- Human compiles project
- Verify: All ~38 action classes compile and link
- Human reviews action organization
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 1.7: Update DataController base class interface

**File**: `pserv5/core/data_controller.h`

**Purpose**: Add new GetActions() method alongside old methods.

**Tasks**:
1. Add `#include "pserv5/core/data_action.h"` to data_controller.h
2. Add new pure virtual method: `virtual std::vector<std::shared_ptr<DataAction>> GetActions() const = 0;`
3. DO NOT remove or modify existing action methods (GetAvailableActions, GetActionName, DispatchAction)
4. Add comment explaining transition period

**🛑 STOP - CHECKPOINT 1.7**
- Human compiles project
- Verify: Compilation fails with "GetActions not implemented" errors (expected!)
- Human reviews interface addition
- **WAIT FOR "CONTINUE" COMMAND**

---

## Phase 2: Implement Actions in Controllers

**Milestone Target**: M2 - All Controllers Use Action Objects

### Step 2.1A: Add action registry to ServicesDataController (header only)

**File**: `pserv5/controllers/services_data_controller.h`

**Purpose**: Add member variables and methods for action registry.

**Tasks**:
1. Add `#include "pserv5/actions/service_actions.h"`
2. Add private member: `std::vector<std::shared_ptr<DataAction>> m_actions;`
3. Add private method declaration: `void InitializeActions();`
4. Add public method override: `std::vector<std::shared_ptr<DataAction>> GetActions() const override;`
5. Keep all old action methods (GetAvailableActions, GetActionName, DispatchAction)

**🛑 STOP - CHECKPOINT 2.1A**
- Human compiles project
- Verify: Compiles (linking will fail - expected)
- Human reviews header changes
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 2.1B: Implement ServicesDataController::GetActions() and InitializeActions()

**File**: `pserv5/controllers/services_data_controller.cpp`

**Purpose**: Create action registry and implement GetActions().

**Tasks**:
1. In constructor, call `InitializeActions();`
2. Implement InitializeActions() to create all 17 action objects with std::make_shared
3. Implement GetActions() to return m_actions
4. Keep old methods unchanged

**Example InitializeActions()**:
```cpp
void ServicesDataController::InitializeActions() {
    m_actions = {
        std::make_shared<ServiceStartAction>(),
        std::make_shared<ServiceStopAction>(),
        std::make_shared<ServiceRestartAction>(),
        std::make_shared<DataActionSeparator>(),
        std::make_shared<ServicePauseAction>(),
        std::make_shared<ServiceResumeAction>(),
        // ... etc
    };
}

std::vector<std::shared_ptr<DataAction>> ServicesDataController::GetActions() const {
    return m_actions;
}

// Implement compatibility shims for old interface:
std::vector<int32_t> ServicesDataController::GetAvailableActions(const DataObject* object) const {
    std::vector<int32_t> result;
    for (const auto& action : m_actions) {
        if (action->IsAvailableFor(object)) {
            result.push_back(action->GetId());
        }
    }
    return result;
}

std::string ServicesDataController::GetActionName(int32_t actionId) const {
    for (const auto& action : m_actions) {
        if (action->GetId() == actionId) {
            return action->GetName();
        }
    }
    return "";
}

void ServicesDataController::DispatchAction(int32_t actionId, DataActionDispatchContext& context) {
    for (const auto& action : m_actions) {
        if (action->GetId() == actionId) {
            action->Execute(context);
            return;
        }
    }
}
```

**🛑 STOP - CHECKPOINT 2.1B**
- Human compiles project
- Verify: Project compiles and links
- Human reviews action registry initialization
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 2.1C: Implement backward compatibility shims in ServicesDataController

**File**: `pserv5/controllers/services_data_controller.cpp`

**Purpose**: Make old enum-based interface delegate to new action objects.

**Tasks**:
1. Implement GetAvailableActions() to filter m_actions by IsAvailableFor()
2. Implement GetActionName() to lookup action by ID
3. Implement DispatchAction() to find and Execute() the action
4. Test that context menus still work with old interface

**🛑 STOP - CHECKPOINT 2.1C**
- Human compiles and runs project
- Human tests: Right-click on service → context menu appears with all actions
- Human tests: Execute Start/Stop → verify they work (Execute() stubs may do nothing yet)
- Verify: **No regression in UI behavior**
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 2.2A-E: Repeat Steps 2.1A-C for remaining controllers (one at a time)

**Repeat Pattern for EACH Controller**:
- **Step 2.2A**: ProcessesDataController header changes (actions registry)
- **Step 2.2B**: ProcessesDataController implementation (InitializeActions, GetActions)
- **Step 2.2C**: ProcessesDataController backward compatibility shims
- **🛑 CHECKPOINT**: Compile, test process actions
- **Step 2.2D**: WindowsDataController (same pattern)
- **🛑 CHECKPOINT**: Compile, test window actions
- **Step 2.2E**: UninstallerDataController (same pattern)
- **🛑 CHECKPOINT**: Compile, test uninstaller actions
- **Step 2.2F**: ModulesDataController (same pattern)
- **🛑 CHECKPOINT**: Compile, test module actions

**After ALL controllers updated → MILESTONE M2 VERIFICATION**

---

### Step 2.3A: Extract ServiceStartAction::Execute() code

**File**: `pserv5/actions/service_actions.cpp`

**Purpose**: Move action logic from controller switch statement into action class.

**Tasks**:
1. Open `pserv5/controllers/services_data_controller.cpp`
2. Locate `case ServiceAction::Start:` in DispatchAction()
3. Copy the entire case block code
4. Paste into ServiceStartAction::Execute()
5. Adjust variable access (context.m_selectedObjects, context.m_pAsyncOp, etc.)
6. Leave old code in place (don't delete yet)

**🛑 STOP - CHECKPOINT 2.3A**
- Human compiles project
- Human tests: Start action from context menu
- Verify: Action works identically to before
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 2.3B-Q: Extract remaining service action implementations (one at a time)

**Process for EACH remaining action** (ServiceStop, ServiceRestart, ServicePause, etc.):
1. Extract Execute() code from switch statement
2. Test that specific action
3. Checkpoint after every 3-4 actions

**Checkpoint after**:
- ServiceStop, ServiceRestart, ServicePause (**CHECKPOINT 2.3B**)
- ServiceResume, SetStartupAuto, SetStartupManual, SetStartupDisabled (**CHECKPOINT 2.3C**)
- CopyName, CopyDisplayName, CopyBinaryPath (**CHECKPOINT 2.3D**)
- OpenInRegistry, OpenInExplorer, OpenTerminal (**CHECKPOINT 2.3E**)
- Uninstall, DeleteRegistryKey, Properties (**CHECKPOINT 2.3F**)

---

### Step 2.4: Extract action implementations for other controllers

**Repeat Step 2.3 pattern for**:
- Process actions (**CHECKPOINT 2.4A-C**)
- Window actions (**CHECKPOINT 2.4D-F**)
- Uninstaller actions (**CHECKPOINT 2.4G**)
- Module actions (**CHECKPOINT 2.4H**)

**After ALL action implementations complete → MILESTONE M2 VERIFICATION**

---

## Phase 3: Create Common Properties Dialog

**Milestone Target**: M3 - Common Properties Dialog Framework Ready

### Step 3.1A: Create CommonPropertiesDialog header file

**File**: `pserv5/dialogs/common_properties_dialog.h` (NEW)

**Purpose**: Define generic properties dialog class.

**Tasks**:
1. Create file `pserv5/dialogs/common_properties_dialog.h`
2. Add necessary includes (data_object.h, data_action.h, functional, vector, memory)
3. Define TabContentRenderer function signature
4. Define CommonPropertiesDialog class with all members and methods

**Implementation**:
```cpp
#pragma once
#include <vector>
#include <memory>
#include <functional>
#include "pserv5/core/data_object.h"
#include "pserv5/core/data_action.h"

class DataController;
struct DataActionDispatchContext;

// Callback signature for rendering tab content
using TabContentRenderer = std::function<void(int tabIndex)>;

class CommonPropertiesDialog {
private:
    bool m_bOpen{false};
    int m_activeTabIndex{0};

    DataController* m_pController{nullptr};
    std::vector<const DataObject*> m_objects;  // Snapshots or live objects (controller decides)
    std::vector<std::shared_ptr<DataAction>> m_actions;  // Filtered to properties dialog only

    TabContentRenderer m_contentRenderer;  // Controller provides this

    void RenderTabContent(int tabIndex);
    void RenderActionButtons(int tabIndex);

public:
    CommonPropertiesDialog() = default;
    ~CommonPropertiesDialog() = default;

    void Open(
        DataController* controller,
        const std::vector<const DataObject*>& objects,
        TabContentRenderer contentRenderer
    );

    void Close();
    bool IsOpen() const { return m_bOpen; }
    void Render(DataActionDispatchContext& context);

    // Access for content renderer
    const DataObject* GetObject(int tabIndex) const;
};
```

**Key Design Points**:
- Controller passes in objects (can be snapshots or live objects)
- Controller passes in callback for rendering tab content
- Dialog manages tab structure and action buttons
- Dialog filters actions to PropertiesDialog visibility

**🛑 STOP - CHECKPOINT 3.1A**
- Human compiles project
- Verify: Header compiles without errors
- Human reviews class design
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 3.1B: Implement CommonPropertiesDialog::Open() and Close()

**File**: `pserv5/dialogs/common_properties_dialog.cpp` (NEW)

**Purpose**: Implement dialog lifecycle methods.

**Tasks**:
1. Create `pserv5/dialogs/common_properties_dialog.cpp`
2. Implement Open() - store controller, objects, callback, filter actions by visibility
3. Implement Close() - clear all state
4. Implement GetObject() accessor
5. Leave Render(), RenderTabContent(), RenderActionButtons() as stubs

**🛑 STOP - CHECKPOINT 3.1B**
- Human compiles project
- Verify: Compiles and links successfully
- Human reviews lifecycle implementation
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 3.1C: Implement CommonPropertiesDialog::Render() and tab management

**File**: `pserv5/dialogs/common_properties_dialog.cpp`

**Purpose**: Implement main render loop and tab bar.

**Tasks**:
1. Implement Render() - window setup, tab bar for multi-select, call RenderTabContent()
2. Implement RenderTabContent() stub - just call callback for now
3. Leave RenderActionButtons() as stub

**🛑 STOP - CHECKPOINT 3.1C**
- Human compiles project
- Verify: Compiles successfully
- Human reviews rendering structure
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 3.1D: Implement CommonPropertiesDialog::RenderActionButtons()

**File**: `pserv5/dialogs/common_properties_dialog.cpp`

**Purpose**: Implement action button rendering logic.

**Tasks**:
1. Implement RenderActionButtons() - loop through actions, check availability, render buttons
2. Handle destructive action coloring
3. Handle separators
4. Execute actions and update context

**Implementation**:
```cpp
void CommonPropertiesDialog::Open(
    DataController* controller,
    const std::vector<const DataObject*>& objects,
    TabContentRenderer contentRenderer)
{
    m_bOpen = true;
    m_activeTabIndex = 0;
    m_pController = controller;
    m_objects = objects;
    m_contentRenderer = contentRenderer;

    // Filter actions to those visible in properties dialog
    m_actions.clear();
    for (const auto& action : controller->GetActions()) {
        auto visibility = action->GetVisibility();
        if ((static_cast<int>(visibility) & static_cast<int>(ActionVisibility::PropertiesDialog)) != 0) {
            m_actions.push_back(action);
        }
    }
}

void CommonPropertiesDialog::Close() {
    m_bOpen = false;
    m_pController = nullptr;
    m_objects.clear();
    m_actions.clear();
    m_contentRenderer = nullptr;
}

void CommonPropertiesDialog::Render(DataActionDispatchContext& context) {
    if (!m_bOpen) return;

    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Properties", &m_bOpen, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Tab bar for multiple selections
    if (m_objects.size() > 1) {
        if (ImGui::BeginTabBar("PropertiesTabs")) {
            for (size_t i = 0; i < m_objects.size(); ++i) {
                std::string tabLabel = m_pController->GetItemName() + " " + std::to_string(i + 1);
                if (ImGui::BeginTabItem(tabLabel.c_str())) {
                    m_activeTabIndex = static_cast<int>(i);
                    RenderTabContent(m_activeTabIndex);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    } else {
        RenderTabContent(0);
    }

    ImGui::End();

    if (!m_bOpen) {
        Close();
    }
}

void CommonPropertiesDialog::RenderTabContent(int tabIndex) {
    // Let controller render the content
    if (m_contentRenderer) {
        m_contentRenderer(tabIndex);
    }

    // Render action buttons for this tab
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    RenderActionButtons(tabIndex);
}

void CommonPropertiesDialog::RenderActionButtons(int tabIndex) {
    const DataObject* object = m_objects[tabIndex];

    for (const auto& action : m_actions) {
        if (!action->IsAvailableFor(object)) {
            continue;  // Skip unavailable actions
        }

        if (action->IsSeparator()) {
            ImGui::Spacing();
            continue;
        }

        // Color destructive actions red
        if (action->IsDestructive()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        }

        if (ImGui::Button(action->GetName().c_str())) {
            // Build context with just this object
            DataActionDispatchContext actionContext = context;
            actionContext.m_selectedObjects = {object};

            // Execute action
            action->Execute(actionContext);

            // If action modified async op, update main context
            context.m_pAsyncOp = actionContext.m_pAsyncOp;
            context.m_bShowProgressDialog = actionContext.m_bShowProgressDialog;
        }

        if (action->IsDestructive()) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
    }

    ImGui::NewLine();
}

const DataObject* CommonPropertiesDialog::GetObject(int tabIndex) const {
    if (tabIndex >= 0 && tabIndex < static_cast<int>(m_objects.size())) {
        return m_objects[tabIndex];
    }
    return nullptr;
}
```

**🛑 STOP - CHECKPOINT 3.1D**
- Human compiles project
- Verify: Compiles successfully
- Human reviews button rendering logic
- **WAIT FOR "CONTINUE" COMMAND**

**After Step 3.1D complete → MILESTONE M3 VERIFICATION**

---

## Phase 4: Integrate with Controllers

**Milestone Target**: M4 - All Controllers Use Common Dialog

### Step 4.1A: Update ServicesDataController header for CommonPropertiesDialog

**File**: `pserv5/controllers/services_data_controller.h`

**Purpose**: Replace old dialog with common dialog.

**Tasks**:
1. Add `#include "pserv5/dialogs/common_properties_dialog.h"`
2. Remove `#include "pserv5/dialogs/service_properties_dialog.h"` (comment out, don't delete yet)
3. Change `std::unique_ptr<ServicePropertiesDialog> m_pPropertiesDialog` to `std::unique_ptr<CommonPropertiesDialog>`
4. Add new method: `void RenderPropertiesContent(int tabIndex);`
5. Add new method: `void OpenPropertiesDialog(const std::vector<const DataObject*>& objects);`

**🛑 STOP - CHECKPOINT 4.1A**
- Human compiles project
- Verify: Compilation fails (expected - RenderPropertiesDialog needs update)
- Human reviews header changes
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 4.1B: Implement ServicesDataController::OpenPropertiesDialog()

**File**: `pserv5/controllers/services_data_controller.cpp`

**Purpose**: Open common dialog with callback.

**Tasks**:
1. Implement OpenPropertiesDialog() - calls m_pPropertiesDialog->Open() with lambda callback
2. Lambda captures `this` and calls RenderPropertiesContent(tabIndex)
3. Leave RenderPropertiesContent() as stub for now

**Example**:
```cpp
void ServicesDataController::OpenPropertiesDialog(const std::vector<const DataObject*>& objects) {
    m_pPropertiesDialog->Open(
        this,
        objects,
        [this](int tabIndex) { this->RenderPropertiesContent(tabIndex); }
    );
}

void ServicesDataController::RenderPropertiesContent(int tabIndex) {
    // TODO: Will implement in next step
    ImGui::Text("Placeholder for service properties");
}
```

**🛑 STOP - CHECKPOINT 4.1B**
- Human compiles project
- Verify: Compiles successfully
- Human reviews OpenPropertiesDialog implementation
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 4.1C: Update ServicePropertiesAction to use OpenPropertiesDialog()

**File**: `pserv5/actions/service_actions.cpp`

**Purpose**: Make Properties action open the new dialog.

**Tasks**:
1. Update ServicePropertiesAction::Execute()
2. Cast context.m_pController to ServicesDataController*
3. Call controller->OpenPropertiesDialog(context.m_selectedObjects)

**🛑 STOP - CHECKPOINT 4.1C**
- Human compiles and runs project
- Human tests: Right-click service → Properties → verify dialog opens (shows placeholder)
- Verify: Dialog has tabs for multi-select
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 4.1D: Implement ServicesDataController::RenderPropertiesContent()

**File**: `pserv5/controllers/services_data_controller.cpp`

**Purpose**: Move property rendering from old dialog.

**Tasks**:
1. Open old `service_properties_dialog.cpp`
2. Copy all property rendering code from Render() method
3. Paste into RenderPropertiesContent()
4. Adjust for new API (GetObject(tabIndex) instead of m_serviceStates[tabIndex])
5. Handle edit buffers if service dialog is editable

**🛑 STOP - CHECKPOINT 4.1D**
- Human compiles and runs project
- Human tests: Open service properties → verify ALL fields render correctly
- Human tests: Edit fields if applicable → Apply → verify changes work
- Human tests: Multi-select → verify tabs work
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 4.2A-D: Migrate ProcessesDataController to CommonPropertiesDialog

**Step 4.2A**: Update header (same as 4.1A)
**Step 4.2B**: Implement OpenPropertiesDialog() - NO snapshots for now, just pass live objects
**Step 4.2C**: Update ProcessPropertiesAction::Execute()
**Step 4.2D**: Implement RenderPropertiesContent() - copy from old dialog

**🛑 STOP - CHECKPOINT 4.2D**
- Human compiles and runs project
- Human tests: Open process properties → verify all fields render
- Human tests: Multi-select processes → verify tabs
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 4.3A-D: Migrate WindowsDataController to CommonPropertiesDialog

**Repeat pattern 4.2A-D** for windows.

**🛑 STOP - CHECKPOINT 4.3D**
- Human tests: Open window properties → verify all fields
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 4.4A-D: Migrate UninstallerDataController to CommonPropertiesDialog

**Repeat pattern 4.2A-D** for uninstaller.

**🛑 STOP - CHECKPOINT 4.4D**
- Human tests: Open uninstaller properties → verify all fields
- **WAIT FOR "CONTINUE" COMMAND**

---

### Step 4.5A-D: Implement ModulesDataController properties dialog (FIRST TIME!)

**Step 4.5A**: Update header - add CommonPropertiesDialog member
**Step 4.5B**: Implement OpenPropertiesDialog()
**Step 4.5C**: Implement ModulePropertiesAction::Execute()
**Step 4.5D**: Implement RenderPropertiesContent() with module fields:
- Module Name
- Base Address
- Size
- Path
- Any other relevant module properties

**🛑 STOP - CHECKPOINT 4.5D**
- Human compiles and runs project
- Human tests: Right-click module → Properties → **FIRST TIME THIS WORKS!**
- Verify: Module properties dialog displays correctly
- **WAIT FOR "CONTINUE" COMMAND**

**After Step 4.5D complete → MILESTONE M4 VERIFICATION**

---

## Phase 5: Update MainWindow

### Step 5.1: Update context menu building

**File**: `pserv5/main_window.cpp`

**Current Code** (lines 860-890):
```cpp
// OLD: enum-based
auto actions = controller->GetAvailableActions(dataObject);
for (int32_t actionId : actions) {
    if (actionId == -1) {
        ImGui::Separator();
        continue;
    }
    std::string name = controller->GetActionName(actionId);
    if (ImGui::MenuItem(name.c_str())) {
        controller->DispatchAction(actionId, dispatchContext);
    }
}
```

**New Code**:
```cpp
// NEW: action-object based
auto allActions = controller->GetActions();
const auto* dataObject = ...; // current or selected object

for (const auto& action : allActions) {
    // Filter to context menu actions
    auto visibility = action->GetVisibility();
    if ((static_cast<int>(visibility) & static_cast<int>(ActionVisibility::ContextMenu)) == 0) {
        continue;
    }

    // Check availability
    if (!action->IsAvailableFor(dataObject)) {
        continue;
    }

    // Handle separators
    if (action->IsSeparator()) {
        ImGui::Separator();
        continue;
    }

    // Render menu item
    std::string label = action->GetName();
    if (dispatchContext.m_selectedObjects.size() > 1) {
        label += " (" + std::to_string(dispatchContext.m_selectedObjects.size()) + " selected)";
    }

    if (ImGui::MenuItem(label.c_str())) {
        action->Execute(dispatchContext);
    }
}
```

**Purpose**: Switch from enum-based to action-object based menu building.

### Step 5.2: Clean up after async operations

**Current Pattern**: MainWindow manages AsyncOperation lifecycle after action dispatch.

**Review**: Ensure actions can create AsyncOperation and MainWindow properly cleans up.

**Verify**:
- Actions can allocate new AsyncOperation via context.m_pAsyncOp
- Actions set context.m_bShowProgressDialog if needed
- MainWindow renders progress dialog when flag is set
- MainWindow cleans up AsyncOperation when complete

**No Changes Expected**: Current pattern should work with action objects.

---

## Phase 6: Remove Old Code

### Step 6.1: Remove old dialog implementations

**Files to Delete**:
- `pserv5/dialogs/service_properties_dialog.h`
- `pserv5/dialogs/service_properties_dialog.cpp`
- `pserv5/dialogs/process_properties_dialog.h`
- `pserv5/dialogs/process_properties_dialog.cpp`
- `pserv5/dialogs/window_properties_dialog.h`
- `pserv5/dialogs/window_properties_dialog.cpp`
- `pserv5/dialogs/uninstaller_properties_dialog.h`
- `pserv5/dialogs/uninstaller_properties_dialog.cpp`

**Process**:
1. Verify all functionality moved to CommonPropertiesDialog + controller callbacks
2. Verify no remaining references to old dialog classes
3. Delete files
4. Update CMakeLists.txt to remove deleted files

### Step 6.2: Remove old action enums and methods

**Files to Update**: All controller headers

**Remove**:
```cpp
// Remove action enums
enum class ServiceAction : int32_t { ... };
enum class ProcessAction : int32_t { ... };
// etc.

// Remove old interface methods
virtual std::vector<int32_t> GetAvailableActions(const DataObject* object) const override;
virtual std::string GetActionName(int32_t actionId) const override;
virtual void DispatchAction(int32_t actionId, DataActionDispatchContext& context) override;
```

**Process**:
1. Verify MainWindow no longer calls old methods
2. Remove enum definitions
3. Remove old method declarations from headers
4. Remove old method implementations from cpp files
5. Remove compatibility shims

### Step 6.3: Clean up includes and forward declarations

**Files**: All headers that included old dialog headers

**Process**:
1. Remove `#include "pserv5/dialogs/xxx_properties_dialog.h"`
2. Add `#include "pserv5/dialogs/common_properties_dialog.h"`
3. Add `#include "pserv5/core/data_action.h"` where needed
4. Remove unused forward declarations

---

## Phase 7: Testing and Refinement

### Step 7.1: Test all actions in context menus

**Test Cases**:
- Right-click on single service → verify all applicable actions appear
- Right-click on stopped service → verify Start appears, Stop does not
- Right-click on running service → verify Stop appears, Start does not
- Test all service actions (Start, Stop, Restart, Pause, Resume, etc.)
- Repeat for processes, windows, uninstaller items
- Test separators appear correctly
- Test "(X selected)" label for multi-select

### Step 7.2: Test all properties dialogs

**Test Cases**:
- Open properties for single service → verify all fields render
- Open properties for multiple services → verify tabs work
- Click action buttons in service properties → verify they execute
- Test read-only dialogs (process, window, uninstaller)
- Test modules properties dialog (new functionality)
- Verify destructive actions show in red
- Test that unavailable actions don't show buttons

### Step 7.3: Test editing functionality (Services only)

**Test Cases**:
- Edit service DisplayName → Apply → verify change persists
- Edit service Description → Apply → verify change persists
- Edit service BinaryPathName → Apply → verify change persists
- Change StartupType → Apply → verify change persists
- Edit multiple services in tabs → Apply each → verify changes
- Close without applying → verify no changes

### Step 7.4: Test async operations

**Test Cases**:
- Start service → verify progress dialog appears
- Stop service → verify progress dialog appears
- Restart service → verify progress dialog appears
- Terminate process → verify progress dialog appears
- Uninstall program → verify progress dialog appears
- Test multi-select async operations
- Verify AsyncOperation cleanup (no leaks)

### Step 7.5: Code review and refinement

**Review Points**:
- Verify no code duplication remains across controllers
- Verify action objects are self-contained
- Verify dialog framework is truly generic
- Check for memory leaks (snapshots, action objects)
- Verify proper const-correctness
- Check for unnecessary includes
- Review for potential simplifications

---

## Phase 8: Documentation and Polish

### Step 8.1: Add code comments

**Files**: All new files

**Purpose**: Document the new architecture for future maintenance.

**Add Comments**:
- Class-level comments explaining purpose and usage
- Method-level comments for non-obvious behavior
- Example usage in comments where helpful
- Note about snapshot vs live object patterns

### Step 8.2: Update TODO comments

**Search**: `TODO` comments related to actions or dialogs

**Actions**:
- Remove TODO in `services_data_controller.h:8` (action objects now implemented)
- Remove TODO in `modules_data_controller.cpp` (properties dialog now implemented)
- Add any new TODOs discovered during implementation

### Step 8.3: Consider future enhancements

**Document Potential Improvements** (in comments or separate doc):
- Action validation before execution (e.g., confirm dangerous operations)
- Action undo/redo support
- Action keyboard shortcuts
- Toolbar buttons for common actions
- Action groups/categories in context menus
- Async action progress reporting improvements
- Action history logging

---

## Success Criteria

The redesign is successful when:

1. ✅ All controllers use CommonPropertiesDialog instead of custom dialogs
2. ✅ All actions are DataAction objects instead of enum cases
3. ✅ Zero code duplication in dialog lifecycle management
4. ✅ Zero code duplication in action dispatch patterns
5. ✅ Modules controller has working properties dialog
6. ✅ All existing functionality preserved
7. ✅ All actions work in both context menus and properties dialogs
8. ✅ Edit support still works for services
9. ✅ Async operations still work correctly
10. ✅ Code is cleaner and more maintainable

---

## Risk Mitigation

**Risk**: Breaking existing functionality during refactor

**Mitigation**:
- Implement new system alongside old system
- Keep backward compatibility shims during transition
- Test thoroughly after each phase
- Don't delete old code until new code is proven

**Risk**: Performance regression from std::shared_ptr and std::function

**Mitigation**:
- Action objects created once at controller initialization
- Action filtering happens on menu open (not per-frame)
- Profile after implementation if concerned

**Risk**: Complexity in edit support

**Mitigation**:
- Keep edit logic in controller, not in generic dialog
- Don't force generic edit abstraction if not needed
- Services controller can maintain edit buffers and dirty tracking

**Risk**: Snapshot memory management

**Mitigation**:
- Document snapshot ownership clearly
- Consider using RefCountedPtr for snapshots
- Or use std::shared_ptr<DataObject> for snapshot objects
- Ensure dialog Close() cleans up snapshots

---

## Implementation Estimate

**Rough Complexity by Phase**:
- Phase 1 (Action Abstraction): ~20 small classes, repetitive code
- Phase 2 (Controller Integration): ~5 controllers × refactoring
- Phase 3 (Common Dialog): 1 new class, moderate complexity
- Phase 4 (Controller Callbacks): ~5 controllers × callback implementation
- Phase 5 (MainWindow Update): Small localized changes
- Phase 6 (Cleanup): Deletion and verification
- Phase 7 (Testing): Comprehensive testing required
- Phase 8 (Documentation): Comments and polish

**Development Approach**: Incremental, one phase at a time, with testing between phases.

---

## Notes

- This design follows the same abstraction pattern as DataObject and DataObjectColumn
- Action objects are lightweight and created once per controller
- The common dialog is truly generic - no controller-specific code
- Specialization remains where it belongs (action behavior, property rendering)
- This design allows future enhancements like keyboard shortcuts, toolbars, action history without major rework