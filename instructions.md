# pserv5 Modernization Plan

## Executive Summary

This document outlines the plan to modernize pserv from version 4 (C# WPF) to version 5 (C++20 native), maintaining full feature parity while improving performance, maintainability, and user experience.

## Current Project Status (as of 2025-11-18)

### ✅ COMPLETED: Phases 1-4 (Milestones 0-27)

**Phase 1: Foundation & Infrastructure** - DONE
- ImGui application with Win32 window and DirectX 11 rendering
- spdlog logging with rotating file sink
- Hierarchical configuration system (TOML-based persistence)
- Tab bar infrastructure with state persistence
- Core interfaces (DataController, DataObject, etc.)

**Phase 2: Windows API Wrappers** - DONE
- WIL integration for Windows API error handling
- ServiceManager with full service control and remote support
- COM-style reference counting (IRefCounted)
- Async operation infrastructure with progress reporting

**Phase 3: Services View (Core Feature)** - DONE
- Complete ServicesDataController with all operations
- Full service management (start/stop/pause/continue/restart)
- Startup type configuration
- Service properties dialog (multi-service editing)
- Service deletion with registry cleanup
- File system integration (Registry, Explorer, Terminal)
- Filtering, sorting, multi-select, column persistence
- Status bar with statistics

**Phase 4: Devices View** - DONE
- DevicesDataController (inherits from ServicesDataController)
- Filters for kernel and file system drivers
- All service operations work on drivers

**UI Modernization** - DONE
- Custom title bar with Windows 11 accent color integration
- Modern ImGui menu bar (File, View, Help)
- Window controls (minimize, maximize, close)
- DPI-aware rendering

**Architecture Achievements:**
- ✅ Clean controller abstraction (UI-independent business logic)
- ✅ Generic rendering loop works with any DataController
- ✅ No MainWindow knowledge of concrete controller types
- ✅ Controllers fully own their domain logic

### 🏃 IN PROGRESS: Phase 5

**Phase 5: Processes View** - PARTIALLY IMPLEMENTED
- `ProcessesDataController` and `ProcessInfo` classes created
- `ProcessManager` for enumeration implemented
- Basic columns defined (Name, PID, Memory, Priority, etc.)
- Actions defined (Terminate, Set Priority, Open Location)
- **Needs**: Testing, performance tuning, and full feature verification against pserv4.

### 🚧 REMAINING WORK: Phases 6-10

**Phase 6: Windows View** - NOT STARTED
- WindowInfo model + WindowsDataController
- Desktop window enumeration and manipulation
- Operations: show/hide/minimize/maximize/close

**Phase 7: Modules View** - NOT STARTED
- ModuleInfo model + ModulesDataController
- DLL enumeration across all processes

**Phase 8: Uninstaller View** - NOT STARTED
- InstalledProgramInfo model + UninstallerDataController
- Registry-based installed programs listing

**Phase 9: Command-Line Interface** - NOT STARTED
- CommandLineProcessor for automation
- Headless mode support
- XML export/import for all views

**Phase 10: Polish & Beta Release** - NOT STARTED
- Comprehensive testing and optimization
- Memory leak detection and fixing
- WiX MSI installer project
- Documentation and release preparation

**Estimated Completion: ~55% (Phase 5 in progress)**

---

## Project Context

### Historical Background
- **1998 (v1.x)**: Original version using custom Windows library
- **2002 (v2.x)**: Complete rewrite in MFC
- **2010 (v3.x)**: Complete rewrite in C# using Windows Forms
- **2014 (v4.x)**: Complete rewrite in C# using WPF with MahApps.Metro
- **2025 (v5.x)**: Proposed rewrite in C++20 with ImGui

### Current State (pserv4)
pserv4 is a comprehensive Windows system management utility providing:
- **Services Management**: Full control over Windows services (start/stop/pause/continue/restart/configure/uninstall)
- **Device Management**: Kernel and file system driver control
- **Process Monitoring**: Active process management with performance metrics
- **Window Management**: Desktop window control (show/hide/minimize/maximize)
- **Uninstaller**: Installed programs viewer and management
- **Modules Viewer**: DLL/module enumeration across all processes
- **Remote Connectivity**: Connect to and manage services on remote machines
- **Command-Line Interface**: Automation support for all operations
- **XML Export/Import**: Template system for batch operations

## Rationale for Modernization

### Goals
1. **Performance**: Eliminate .NET GC overhead and achieve native performance
2. **Binary Size**: Reduce deployment footprint (no .NET runtime dependency)
3. **Maintainability**: Modern C++20 features for clearer, safer code
4. **UI Flexibility**: ImGui provides immediate-mode simplicity and better theming
5. **Logging**: Replace log4net with modern spdlog
6. **Resource Management**: RAII and smart pointers eliminate manual cleanup

### Technology Stack

#### Core Technologies
- **Language**: C++20 (ISO standard)
- **UI Framework**: ImGui with DirectX 11 backend
- **Logging**: spdlog
- **Configuration**: Custom hierarchical configuration system (from jucyaudio project)
- **Windows API Helpers**: WIL (Windows Implementation Library)
- **JSON Processing**: rapidjson (Available in project)
- **XML Processing**: pugixml (Planned for pserv4 compatibility, not yet added)
- **Build System**: MSBuild (Visual Studio project files)

#### Dependency Management
All dependencies integrated as Git submodules:
- `imgui` - Already added
- `spdlog` - Already added
- `wil` - Already added
- `tomlplusplus` - Already added
- `rapidjson` - Already added
- `pugixml` - To be added

Note: Configuration system classes will be copied from `archive\Config\` directory (no external dependency)

## Architecture Design

### Core Principles
1. **UI Independence**: Controllers contain no UI framework code (no ImGui, no WPF, nothing) - enables future UI ports
2. **Separation of Concerns**: Clear boundaries between data (models), logic (controllers), and presentation (UI layer)
3. **COM-Style Refcounting**: Manual retain/release with debugging support - thread-safe, explicit ownership
4. **Asynchronous Operations**: Long-running operations (service start/stop) run on background threads, don't block UI
5. **MFC Naming Conventions**: Member variables prefixed with `m_`, booleans with `m_b`, pointers with `m_p`
6. **Modern C++20**: Curly brace initialization always, std::async for threading, structured concurrency
7. **UTF-8 Everywhere**: Use `std::string` internally, convert to `std::wstring` only at Windows API boundaries
8. **Single Responsibility**: Each class has one clear purpose
9. **Preserve Proven Patterns**: Evolve the successful DataController/DataObject/DataObjectColumn pattern from pserv4

### Understanding pserv4's Architecture (What We Must Preserve)

The pserv4 architecture is built on a powerful abstraction that has evolved over multiple rewrites. **This is the core pattern we must maintain:**

#### The Three-Part Pattern

**1. DataObject (Data Model)**
- Base class for all displayable items (ServiceInfo, ProcessInfo, WindowInfo, etc.)
- Contains:
  - Properties for each column (Name, DisplayName, Status, etc.)
  - `InternalID` for tracking identity
  - `IsRunning`, `IsDisabled` state flags
  - `INotifyPropertyChanged` for UI updates (WPF-specific)
  - Helper methods: `SetStringProperty()`, `SetRunning()`, `SetDisabled()`

**2. DataObjectColumn (Column Metadata)**
- Simple descriptor pairing display name with property binding
- Properties:
  - `DisplayName`: What user sees ("Display Name")
  - `BindingName`: Property name on DataObject ("DisplayName")
- Used for:
  - Column creation in UI
  - XML export/import (reflection over properties)
  - Dynamic column ordering

**3. DataController (Business Logic)**
- **This is the key abstraction** - it provides everything a view needs:
  - **Data Management**: `Refresh()` - fetches and updates data
  - **Column Definition**: `GetColumns()` - returns all available columns
  - **Action Definition**: `GetAvailableActions()` - returns valid actions for selection
  - **Action Execution**: `ExecuteAction()` - performs operations on items
  - **XML Export/Import**: `ExportToXML()`, `ImportFromXML()`, `ApplyTemplateInfo()`

**Key Insight from pserv4**: The DataController knows the complete behavior of a view, but is **UI framework independent**.

**Refinement for pserv5**: Separate UI concerns from business logic:
- **pserv4**: Controller contained WPF-specific code (ContextMenu, MenuItem, etc.)
- **pserv5**: Controller provides data and actions, but knows nothing about ImGui/WPF/Qt
- **Benefit**: Can port to WinUI, web, or other UI without touching controllers

### Why This Pattern Works

1. **Simplicity**: One controller class per view type - easy to understand
2. **Consistency**: All views work the same way - switch controller, everything updates
3. **Flexibility**: Each controller customizes exactly what it needs via overrides
4. **Reusability**: DevicesDataController inherits from ServicesDataController (devices are just filtered services)
5. **Extensibility**: Adding a new view = create DataObject subclass + DataController subclass

### High-Level Architecture (pserv5 - UI-Independent Controllers)

```
┌─────────────────────────────────────────────────┐
│         UI Layer (ImGui-specific)               │
│  - MainWindow: Win32 window + ImGui loop        │
│  - Renders tables generically                   │
│  - Renders context menus from action list       │
│  - Manages view state (column order, filter)    │
│  - Handles auto-refresh timer                   │
│  - NO business logic                            │
└─────────────────────────────────────────────────┘
                       │
                       │ queries
                       ▼
┌─────────────────────────────────────────────────┐
│    DataController (UI-Independent Logic)        │
│  ONE per view type - provides view behavior:    │
│  - Refresh(vector<DataObject>&)                 │
│  - GetColumns() → span<DataObjectColumn>        │
│  - GetAvailableActions(selection) → actions     │
│  - ExecuteAction(actionId, selection)           │
│  - ExportToXML() / ImportFromXML()              │
│  - GetName(), GetItemName()                     │
│  - NO UI framework code (no ImGui, no WPF)      │
└─────────────────────────────────────────────────┘
                       │
                       │ implemented by
                       ▼
┌─────────────────────────────────────────────────┐
│        Concrete Controllers                     │
│  - ServicesDataController                       │
│  - DevicesDataController (inherits Services)    │
│  - ProcessesDataController                      │
│  - WindowsDataController                        │
│  - ModulesDataController                        │
│  - UninstallerDataController                    │
└─────────────────────────────────────────────────┘
                       │
                       │ manages
                       ▼
┌─────────────────────────────────────────────────┐
│        DataObject (Abstract Base)               │
│  - GetId() → string                             │
│  - Update(const DataObject&)                    │
│  - GetProperty(int propertyId) → string         │
│  - IsRunning(), IsDisabled()                    │
└─────────────────────────────────────────────────┘
                       │
                       │ implemented by
                       ▼
┌─────────────────────────────────────────────────┐
│        Concrete DataObjects (Models)            │
│  - ServiceInfo                                  │
│  - ProcessInfo                                  │
│  - WindowInfo, ModuleInfo, etc.                 │
└─────────────────────────────────────────────────┘
                       │
                       │ uses
                       ▼
┌─────────────────────────────────────────────────┐
│      Windows API Wrappers (Data Sources)        │
│  - ServiceManager                               │
│  - ProcessManager                               │
│  - WindowManager, etc.                          │
└─────────────────────────────────────────────────┘
```

**Key Architectural Decisions**:
1. ✅ **Controllers are UI-independent**: Enables porting to WinUI, Qt, web, etc.
2. ✅ **UI layer is generic**: MainWindow renders any controller the same way
3. ✅ **Separation of concerns**:
   - Controllers: Define what data exists and what actions are possible
   - UI Layer: Decide how to present it and handle user interaction
4. ✅ **Column ordering is UI state**: Controllers provide available columns, UI decides which to show and in what order
5. ✅ **Actions are controller-specific**: Each controller defines its own actions via enums

### Core Classes

#### IRefCounted - Base for Reference Counting

All data objects use COM-style refcounting for thread-safe, explicit lifetime management.

```cpp
// In core/refcounted.h

#pragma once
#include <atomic>
#include <Windows.h>  // For InterlockedIncrement/Decrement

#undef PSERV_REFCOUNT_DEBUGGING
#ifdef PSERV_REFCOUNT_DEBUGGING
#define REFCOUNT_DEBUG_ARGS __FILE__, __LINE__
#define REFCOUNT_DEBUG_SPEC const char* file, int line
#else
#define REFCOUNT_DEBUG_ARGS
#define REFCOUNT_DEBUG_SPEC
#endif

namespace pserv5 {

class IRefCounted {
public:
    IRefCounted() = default;
    virtual ~IRefCounted() = default;

    IRefCounted(const IRefCounted&) = delete;
    IRefCounted& operator=(const IRefCounted&) = delete;
    IRefCounted(IRefCounted&&) = default;
    IRefCounted& operator=(IRefCounted&&) = default;

    /// Thread-safe reference increment
    virtual void retain(REFCOUNT_DEBUG_SPEC) const = 0;

    /// Thread-safe reference decrement. Deletes when count reaches 0.
    virtual void release(REFCOUNT_DEBUG_SPEC) const = 0;
};

// Base implementation
class RefCountImpl : public IRefCounted {
public:
    RefCountImpl() : m_refCount{1} {}

    void retain(REFCOUNT_DEBUG_SPEC) const override final {
        InterlockedIncrement(&m_refCount);
    }

    void release(REFCOUNT_DEBUG_SPEC) const override final {
        if (InterlockedDecrement(&m_refCount) == 0) {
            delete this;
        }
    }

protected:
    virtual ~RefCountImpl() = default;

private:
    mutable std::atomic<long> m_refCount;
};

} // namespace pserv5
```

**Key Points**:
- Thread-safe via `InterlockedIncrement/Decrement`
- Starts at refcount 1 (creator owns it)
- Debug macro can track file/line of retain/release calls
- No smart pointer wrapper - raw pointers with explicit retain/release

### Object Lifetime and Ownership Rules

**Philosophy**: Explicit is better than implicit. COM-style refcounting makes ownership reasoning clear and auditable. No compiler magic, no surprises.

#### Core Rules

**Rule 1: Creator Owns**
When you create an object, you own it (refcount starts at 1). You must eventually `release()` it.

```cpp
auto* pService = new ServiceInfo();  // refcount = 1, we own it
// ... use it ...
pService->release();  // we're done, object may be deleted if refcount hits 0
```

**Rule 2: Return = Transfer Ownership**
When a function returns a pointer, ownership transfers to the caller. The callee does NOT release.

```cpp
ServiceInfo* CreateService() {
    auto* pObj = new ServiceInfo();  // refcount = 1
    return pObj;  // Caller now owns it, we don't release
}

// Caller side:
auto* pService = CreateService();  // We now own it
// ... use it ...
pService->release();  // We must release it
```

**Rule 3: Store = Retain**
If you want to store a pointer (in a member variable, container, etc.), you must `retain()` it. When you're done storing it, `release()` it.

```cpp
class Controller {
    std::vector<ServiceInfo*> m_services;

    void AddService(ServiceInfo* pSvc) {
        pSvc->retain();  // We're storing it
        m_services.push_back(pSvc);
    }

    void Clear() {
        for (auto* pSvc : m_services) {
            pSvc->release();  // We're done storing it
        }
        m_services.clear();
    }
};
```

**Rule 4: Caller Never Touches Refcount for Simple Calls**
When you call a function and just pass a pointer as a parameter, don't retain/release around the call. The callee will retain if it needs to keep the pointer.

```cpp
void ProcessService(ServiceInfo* pSvc);  // Just uses pSvc, doesn't store it

void DoWork() {
    auto* pService = GetService();  // We own it (refcount = 1)
    ProcessService(pService);  // Just passing it, no retain/release needed
    pService->release();  // We're done with it
}
```

#### Common Patterns

**Pattern 1: Vectors of Pointers (Container Owns Contents)**

```cpp
class ServicesDataController {
    std::vector<ServiceInfo*> m_objects;

    void Refresh() {
        // Release old objects (we owned them)
        for (auto* pObj : m_objects) {
            pObj->release();
        }
        m_objects.clear();

        // Get new objects (ownership transferred to us)
        auto newObjects = m_serviceManager.EnumerateServices();  // returns vector<ServiceInfo*>
        m_objects = std::move(newObjects);
        // We now own all objects (each at refcount = 1)
    }

    ~ServicesDataController() {
        // Must release everything we own
        for (auto* pObj : m_objects) {
            pObj->release();
        }
    }
};
```

**Pattern 2: UI Borrowing Pointers (No Refcount Changes)**

```cpp
void MainWindow::RenderServicesTab() {
    // Get reference to controller's objects (we're just borrowing)
    const auto& objects = m_pController->GetObjects();  // returns const vector<ServiceInfo*>&

    // Render without touching refcounts
    for (auto* pObj : objects) {
        ImGui::Text("%s", pObj->GetProperty(ServiceProperty::Name).c_str());
    }
    // Done - no cleanup needed, controller still owns them
}
```

**Pattern 3: UI Storing Selection (Retain/Release)**

```cpp
class MainWindow {
    ServiceInfo* m_pSelectedService{nullptr};

    void SelectService(ServiceInfo* pObj) {
        // Release old selection
        if (m_pSelectedService) {
            m_pSelectedService->release();
        }

        // Store new selection
        m_pSelectedService = pObj;
        if (m_pSelectedService) {
            m_pSelectedService->retain();  // We're keeping a reference
        }
    }

    ~MainWindow() {
        if (m_pSelectedService) {
            m_pSelectedService->release();
        }
    }
};
```

**Pattern 4: Async Operations (Retain Before Launch, Release When Done)**

```cpp
void ServicesDataController::StartService(ServiceInfo* pSvc) {
    pSvc->retain();  // Keep object alive during async work

    std::async([pSvc]() {
        // Do long-running work...
        StartServiceInternal(pSvc->GetId());

        pSvc->release();  // Done with our reference
    });
    // pSvc might get deleted by controller during async work,
    // but our retain keeps it alive until we're done
}
```

#### Why This Works

**Benefits over smart pointers:**
1. **Explicit reasoning**: Every retain/release is visible. Grep finds them all.
2. **No magic**: Compiler doesn't guess when to increment/decrement.
3. **Debuggable**: Can log every retain/release with file/line (via debug macros).
4. **No circular reference footguns**: You see the retain, you know to release.
5. **Battle-tested**: COM has used this for 30+ years across billions of Windows systems.

**Thread safety**: `InterlockedIncrement/Decrement` are atomic. Multiple threads can safely retain/release the same object.

**Memory leaks**: If you follow the rules, leaks are impossible. Every `new` creates refcount=1. Every retain increments. Every release decrements. When it hits 0, `delete this`.

**Debugging tip**: Enable `USE_REFCOUNT_DEBUGGING` to log file/line of every retain/release:
```cpp
#define USE_REFCOUNT_DEBUGGING  // In debug builds
pObj->retain(REFCOUNT_DEBUG_ARGS);  // Logs: "Retained at file.cpp:123"
```

#### DataObjectColumn
Simple struct pairing display name with property ID.

```cpp
struct DataObjectColumn {
    std::string displayName;  // What user sees: "Display Name"
    int propertyId;           // Property identifier from controller-specific enum

    DataObjectColumn(std::string display, int id)
        : displayName{std::move(display)}, propertyId{id} {}
};
```

**Why int instead of string?**
- Performance: Switch statement vs string comparisons
- Type safety: Controller-specific enums prevent typos
- Refactoring: Compiler catches all property uses
- Flexibility: Each controller defines its own property enum

**Usage**: Controllers return a list of these to define available columns. UI layer decides which to show and in what order.

#### DataObject (Abstract Base Class)
Base class for all data items (services, processes, windows, etc.). Uses refcounting for lifetime management.

```cpp
class DataObject : public RefCountImpl {
private:
    bool m_bIsRunning{false};
    bool m_bIsDisabled{false};

protected:
    void SetRunning(bool bRunning) { m_bIsRunning = bRunning; }
    void SetDisabled(bool bDisabled) { m_bIsDisabled = bDisabled; }

public:
    virtual ~DataObject() = default;

    // Core identity and lifecycle
    virtual std::string GetId() const = 0;
    virtual void Update(const DataObject& other) = 0;

    // Property access by integer ID (UI-agnostic)
    virtual std::string GetProperty(int propertyId) const = 0;
    virtual void SetProperty(int propertyId, std::string_view value) = 0;

    // State flags (used for styling in UI)
    bool IsRunning() const { return m_bIsRunning; }
    bool IsDisabled() const { return m_bIsDisabled; }

    // Optional filename (for "Show Properties" context menu)
    virtual std::string GetFileName() const { return ""; }
};
```

**Key Design Points**:
- **Refcounting**: Inherits from `RefCountImpl` - use `retain()`/`release()` for lifetime
- **MFC Naming**: `m_bIsRunning`, `m_bIsDisabled` - clear, proven convention
- **Curly Brace Init**: All members initialized with `{}`
- **Protected Setters**: Private state with protected setters - allows base class validation later
- **`GetProperty(int)`**: Takes integer ID, not string. Each controller defines its own enum, casts to int.
- **Generic consumers**: UI layer, XML exporter, etc. work with `int` - they don't need to know controller types
- **Type safety within controllers**: Each controller uses its own enum internally
- **Performance**: Switch statements compile to jump tables
- **`Update()`**: Enables efficient in-place updates during refresh
- **State flags**: Control visual styling (grayed out, highlighted, etc.)

**Lifetime Management Example**:
```cpp
// Creating
auto* pService = new ServiceInfo{};  // Refcount starts at 1

// Storing
std::vector<DataObject*> m_items;
m_items.push_back(pService);
pService->retain(REFCOUNT_DEBUG_ARGS);  // Now refcount is 2

// Cleanup
for (auto* pItem : m_items) {
    pItem->release(REFCOUNT_DEBUG_ARGS);
}
m_items.clear();
```

#### Action
Descriptor for an action that can be performed on selected items.

```cpp
struct Action {
    std::string displayName;  // "Start Service", "Terminate Process", etc.
    int actionId;             // From controller-specific enum, cast to int
    std::string iconName;     // Optional: "play", "stop", "delete", etc.
    bool isSeparator = false;

    Action(std::string display, int id, std::string icon = "")
        : displayName(std::move(display)), actionId(id), iconName(std::move(icon)) {}

    // Factory for separator
    static Action Separator() {
        Action a("", -1);
        a.isSeparator = true;
        return a;
    }
};
```

**Why Actions Work Like Columns**:
- Each controller defines its own action enum (ServiceAction, ProcessAction, etc.)
- Actions are opaque integers to the UI layer
- Controllers decide which actions are valid based on selection
- UI just renders the list - no business logic

#### Asynchronous Operations

Long-running operations (starting services, querying remote machines, etc.) must not block the UI thread.

**Architecture**:
- Operations run on background threads via `std::async`
- Progress/completion reported back to UI thread via Windows message queue
- Cancellation supported (best-effort - some operations can't be interrupted)
- One operation at a time with modal progress dialog

```cpp
// Result structure posted to UI thread
struct AsyncOperationResult {
    int operationId{0};
    bool bIsProgressUpdate{false};
    float progress{0.0f};
    std::string statusMessage;
    bool bSuccess{false};
    std::string errorMessage;
};

// Custom Windows message
#define WM_ASYNC_OPERATION_COMPLETE (WM_USER + 1)

class AsyncOperation {
public:
    enum class Status { Pending, Running, Completed, Cancelled, Failed };

private:
    std::atomic<Status> m_status{Status::Pending};
    std::atomic<bool> m_bCancelRequested{false};
    std::future<void> m_future;
    int m_operationId{0};
    std::string m_description;
    MainWindow* m_pMainWindow{nullptr};

public:
    AsyncOperation(int id, std::string desc, MainWindow* pWnd)
        : m_operationId{id},
          m_description{std::move(desc)},
          m_pMainWindow{pWnd} {}

    void Start(std::function<void(AsyncOperation*)> workFunc) {
        m_status = Status::Running;

        m_future = std::async(std::launch::async, [this, workFunc]() {
            try {
                workFunc(this);

                if (m_bCancelRequested.load()) {
                    m_status = Status::Cancelled;
                } else {
                    m_status = Status::Completed;
                }
            } catch (const std::exception& e) {
                spdlog::error("Operation {} failed: {}", m_operationId, e.what());
                m_status = Status::Failed;
            }
        });
    }

    void RequestCancel() {
        m_bCancelRequested = true;
    }

    bool IsCancelRequested() const {
        return m_bCancelRequested.load();
    }

    void ReportProgress(float progress, std::string message) {
        AsyncOperationResult result{};
        result.operationId = m_operationId;
        result.bIsProgressUpdate = true;
        result.progress = progress;
        result.statusMessage = std::move(message);

        m_pMainWindow->PostAsyncResult(result);
    }

    void ReportCompletion(bool bSuccess, std::string errorMsg = "") {
        AsyncOperationResult result{};
        result.operationId = m_operationId;
        result.bIsProgressUpdate = false;
        result.bSuccess = bSuccess;
        result.errorMessage = std::move(errorMsg);

        m_pMainWindow->PostAsyncResult(result);
    }
};
```

**Communication Pattern**:
```cpp
class MainWindow {
private:
    HWND m_hwnd{nullptr};
    std::mutex m_resultQueueMutex;
    std::queue<AsyncOperationResult> m_resultQueue;

public:
    // Called from worker thread
    void PostAsyncResult(const AsyncOperationResult& result) {
        {
            std::lock_guard<std::mutex> lock{m_resultQueueMutex};
            m_resultQueue.push(result);
        }
        // Wake up UI thread
        ::PostMessage(m_hwnd, WM_ASYNC_OPERATION_COMPLETE, 0, 0);
    }

    // WndProc handler
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_ASYNC_OPERATION_COMPLETE) {
            ProcessAsyncResults();
            return 0;
        }
        return DefWindowProc(m_hwnd, msg, wParam, lParam);
    }

    void ProcessAsyncResults() {
        std::lock_guard<std::mutex> lock{m_resultQueueMutex};

        while (!m_resultQueue.empty()) {
            auto& result = m_resultQueue.front();

            if (result.bIsProgressUpdate) {
                m_operationProgress = result.progress;
                m_operationDescription = result.statusMessage;
            } else {
                m_currentOperationId = -1;
                if (!result.bSuccess) {
                    ShowErrorDialog(result.errorMessage);
                }
                // Trigger refresh
                if (m_pCurrentController) {
                    m_pCurrentController->Refresh(m_items);
                }
            }

            m_resultQueue.pop();
        }
    }

    void RenderFrame() {
        // Show progress dialog if operation active
        if (m_currentOperationId >= 0) {
            ImGui::OpenPopup("Operation In Progress");

            if (ImGui::BeginPopupModal("Operation In Progress", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("%s", m_operationDescription.c_str());
                ImGui::ProgressBar(m_operationProgress);

                if (ImGui::Button("Cancel")) {
                    CancelCurrentOperation();
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }
    }
};
```

**Cancellation Notes**:
- Best-effort only - some Windows APIs block and can't be interrupted
- Controllers check `IsCancelRequested()` between operations
- Example: Starting a service polls status every 100ms, checks for cancellation
- If cancelled mid-operation, cleanup still happens (via `retain()`/`release()`)

#### DataController (Abstract Base Class)
**The heart of the architecture** - UI-independent business logic.

```cpp
class DataController {
protected:
    MainWindow* m_pMainWindow{nullptr};
    std::atomic<int> m_nextOperationId{1};
    std::string m_lastErrorMessage;

public:
    virtual ~DataController() = default;

    // ===== IDENTITY =====
    virtual std::string_view GetName() const = 0;        // "Services", "Processes", etc.
    virtual std::string_view GetItemName() const = 0;    // "Service", "Process", etc. (for XML)

    // ===== DATA MANAGEMENT =====
    // Refresh the items vector with current system state
    // Uses RefreshManager internally to update efficiently
    virtual void Refresh(std::vector<DataObject*>& items) = 0;

    // ===== COLUMN DEFINITION =====
    // Returns all available columns for this view
    // UI layer decides which to show and in what order
    virtual std::span<const DataObjectColumn> GetColumns() const = 0;

    // ===== ACTION SYSTEM =====
    // Returns available actions for current selection
    // Controller decides which actions are valid
    virtual std::vector<Action> GetAvailableActions(
        const std::vector<DataObject*>& selectedItems) const = 0;

    // Execute an action on selected items (async for long operations)
    // Returns false if can't start operation, true if started
    // Actual result comes via message queue
    virtual bool ExecuteActionAsync(int actionId,
                                    const std::vector<DataObject*>& selectedItems) = 0;

    // Error reporting
    std::string GetLastErrorMessage() const { return m_lastErrorMessage; }

    // ===== XML IMPORT/EXPORT =====
    bool ExportToXML(const std::filesystem::path& path,
                     const std::vector<DataObject*>& items);
    bool ImportFromXML(const std::filesystem::path& path);
    virtual void ApplyTemplateInfo(const TemplateInfo& info) {}

    // Called by MainWindow
    void SetMainWindow(MainWindow* pWnd) { m_pMainWindow = pWnd; }

protected:
    DataController(std::string name, std::string itemName)
        : m_name{std::move(name)}, m_itemName{std::move(itemName)} {}

    void SetLastError(std::string msg) {
        m_lastErrorMessage = std::move(msg);
    }

private:
    std::string m_name;
    std::string m_itemName;
};
```

**CRITICAL**: This class contains **NO UI framework code**. No ImGui, no WPF, nothing.
- ✅ Can be reused with any UI framework
- ✅ Can be unit tested without UI
- ✅ Can be used from command-line tools
- ✅ Business logic in one place
- ✅ Async operations don't block

### Concrete Example: ServicesDataController

To clarify the pattern, here's how ServicesDataController would be implemented:

```cpp
// Controller-specific enums
enum class ServiceProperty {
    Name,
    DisplayName,
    Status,
    StartType,
    ProcessId,
    BinaryPath,
    Description
};

enum class ServiceAction {
    Start,
    Stop,
    Restart,
    Pause,
    Continue,
    ChangeStartupType,
    Uninstall,
    OpenRegistryKey,
    StartCmdInPath
};

// Data model - UTF-8 everywhere
class ServiceInfo : public DataObject {
private:
    std::string m_name;          // UTF-8
    std::string m_displayName;   // UTF-8
    std::string m_status;        // UTF-8
    std::string m_startType;     // UTF-8
    DWORD m_processId{0};

public:
    std::string GetId() const override {
        return m_name;  // Already UTF-8
    }

    void Update(const DataObject& other) override {
        auto& svc = static_cast<const ServiceInfo&>(other);
        m_displayName = svc.m_displayName;
        m_status = svc.m_status;
        m_processId = svc.m_processId;
        SetRunning(svc.IsRunning());
        SetDisabled(svc.IsDisabled());
    }

    std::string GetProperty(int propertyId) const override {
        switch (static_cast<ServiceProperty>(propertyId)) {
            case ServiceProperty::Name:
                return m_name;
            case ServiceProperty::DisplayName:
                return m_displayName;
            case ServiceProperty::Status:
                return m_status;
            case ServiceProperty::StartType:
                return m_startType;
            case ServiceProperty::ProcessId:
                return std::to_string(m_processId);
            default:
                return "";
        }
    }

    // Setters for construction (UTF-8)
    void SetName(std::string name) { m_name = std::move(name); }
    void SetDisplayName(std::string dn) { m_displayName = std::move(dn); }
    // ... etc

    // Accessors for action execution (UTF-8)
    const std::string& GetName() const { return m_name; }
};

// Controller - UI-INDEPENDENT
class ServicesDataController : public DataController {
private:
    ServiceManager serviceManager_;  // Windows API wrapper
    std::vector<DataObjectColumn> columns_;

public:
    ServicesDataController()
        : DataController("Services", "Service")
    {
        columns_ = {
            {"Name", static_cast<int>(ServiceProperty::Name)},
            {"Display Name", static_cast<int>(ServiceProperty::DisplayName)},
            {"Status", static_cast<int>(ServiceProperty::Status)},
            {"Startup Type", static_cast<int>(ServiceProperty::StartType)},
            {"Process ID", static_cast<int>(ServiceProperty::ProcessId)}
        };
    }

    std::string_view GetName() const override { return "Services"; }
    std::string_view GetItemName() const override { return "Service"; }
    std::span<const DataObjectColumn> GetColumns() const override { return columns_; }

    void Refresh(std::vector<std::shared_ptr<DataObject>>& items) override {
        auto result = serviceManager_.EnumerateServices();
        if (!result) {
            spdlog::error("Failed to enumerate services: {}", result.error().message());
            return;
        }

        RefreshManager<ServiceInfo> refresher(items);
        refresher.Refresh(*result, [](const auto& svcData) {
            auto svc = std::make_shared<ServiceInfo>();
            svc->SetName(svcData.name);
            svc->SetDisplayName(svcData.displayName);
            svc->SetStatus(svcData.status);
            svc->SetRunning(svcData.isRunning);
            return svc;
        });
    }

    std::vector<Action> GetAvailableActions(
        const std::vector<DataObject*>& selectedItems) const override {

        std::vector<Action> actions;

        // Determine valid actions based on selection
        bool canStart = CanStartSelected(selectedItems);
        bool canStop = CanStopSelected(selectedItems);

        if (canStart) {
            actions.push_back({"Start", static_cast<int>(ServiceAction::Start), "play"});
        }
        if (canStop) {
            actions.push_back({"Stop", static_cast<int>(ServiceAction::Stop), "stop"});
        }
        actions.push_back({"Restart", static_cast<int>(ServiceAction::Restart), "repeat"});
        actions.push_back({"Pause", static_cast<int>(ServiceAction::Pause), "pause"});
        actions.push_back({"Continue", static_cast<int>(ServiceAction::Continue), "play"});

        actions.push_back(Action::Separator());

        actions.push_back({"Change Startup Type...",
                          static_cast<int>(ServiceAction::ChangeStartupType)});
        actions.push_back({"Uninstall Service",
                          static_cast<int>(ServiceAction::Uninstall), "delete"});

        actions.push_back(Action::Separator());

        actions.push_back({"Open Registry Key",
                          static_cast<int>(ServiceAction::OpenRegistryKey)});
        actions.push_back({"Start CMD in Path",
                          static_cast<int>(ServiceAction::StartCmdInPath)});

        return actions;
    }

    void ExecuteAction(int actionId,
                      const std::vector<DataObject*>& selectedItems) override {
        switch (static_cast<ServiceAction>(actionId)) {
            case ServiceAction::Start:
                for (auto* item : selectedItems) {
                    auto* svc = static_cast<ServiceInfo*>(item);
                    auto result = serviceManager_.StartService(svc->GetName());
                    if (!result) {
                        spdlog::error("Failed to start {}: {}",
                                     svc->GetId(), result.error().message());
                    }
                }
                break;

            case ServiceAction::Stop:
                for (auto* item : selectedItems) {
                    auto* svc = static_cast<ServiceInfo*>(item);
                    serviceManager_.StopService(svc->GetName());
                }
                break;

            case ServiceAction::ChangeStartupType:
                // This would trigger a dialog in the UI layer
                // Controller just provides the action, UI handles the interaction
                break;

            // ... other cases
        }
    }

private:
    bool CanStartSelected(const std::vector<DataObject*>& items) const {
        return std::any_of(items.begin(), items.end(), [](auto* pItem) {
            return !pItem->IsRunning();
        });
    }

    bool CanStopSelected(const std::vector<DataObject*>& items) const {
        return std::any_of(items.begin(), items.end(), [](auto* pItem) {
            return pItem->IsRunning();
        });
    }

    // Async worker - called on background thread
    bool StartServiceAsync(ServiceInfo* pSvc, AsyncOperation* pOp) {
        // pSvc->GetName() returns UTF-8 string
        // ServiceManager::StartService() converts to wstring at API boundary
        auto result = m_serviceManager.StartService(pSvc->GetName());
        if (!result) {
            SetLastError(result.error().message());
            return false;
        }

        // Poll status until started or cancelled
        for (int i = 0; i < 100; ++i) {  // 10 second timeout
            if (pOp->IsCancelRequested()) {
                SetLastError("Cancelled by user");
                return false;
            }

            auto status = m_serviceManager.QueryServiceStatus(pSvc->GetName());
            if (!status) {
                SetLastError(status.error().message());
                return false;
            }

            if (status->bIsRunning) {
                return true;  // Success!
            }

            // Report progress - all strings UTF-8
            pOp->ReportProgress(i / 100.0f,
                std::format("Waiting for service to start... {}s", i / 10));

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        SetLastError("Service start timeout");
        return false;
    }
};
```

**Key Points**:
- NO ImGui code. NO WPF code. Pure business logic.
- Async operations with progress reporting and cancellation
- Refcounting keeps objects alive during async work
- MFC naming (`pSvc`, `bSuccess`, `m_serviceManager`)

**How MainWindow (UI Layer) Uses This**:

```cpp
// UI Layer - Contains ALL ImGui code
class MainWindow {
private:
    std::map<ViewType, std::unique_ptr<DataController>> controllers_;
    DataController* currentController_ = nullptr;
    std::vector<std::shared_ptr<DataObject>> items_;

    // UI state (NOT in controller)
    std::string filterText_;
    std::vector<int> visibleColumnIndices_;  // Which columns to show
    std::vector<int> columnOrder_;            // What order
    std::vector<DataObject*> selectedItems_;

public:
    void Initialize() {
        // Register all controllers
        controllers_[ViewType::Services] = std::make_unique<ServicesDataController>();
        controllers_[ViewType::Devices] = std::make_unique<DevicesDataController>();
        controllers_[ViewType::Processes] = std::make_unique<ProcessesDataController>();
        // ... etc

        // Switch to initial view
        SwitchToView(ViewType::Services);
    }

    void SwitchToView(ViewType type) {
        items_.clear();
        selectedItems_.clear();
        currentController_ = controllers_[type].get();

        // Load UI state for this controller (column order, etc.)
        LoadViewState(type);

        // Fetch data
        currentController_->Refresh(items_);
    }

    void RenderFrame() {
        // Tab bar for switching controllers (ImGui code)
        if (ImGui::BeginTabBar("Views")) {
            if (ImGui::TabItem("Services")) SwitchToView(ViewType::Services);
            if (ImGui::TabItem("Devices")) SwitchToView(ViewType::Devices);
            if (ImGui::TabItem("Processes")) SwitchToView(ViewType::Processes);
            // ... etc
            ImGui::EndTabBar();
        }

        // Filter text box (ImGui code)
        ImGui::InputText("Filter", &filterText_);

        // Render table generically (ImGui code)
        RenderTable();

        // Render context menu if right-clicked (ImGui code)
        RenderContextMenu();

        // Auto-refresh every 5 seconds
        if (shouldRefresh_) {
            currentController_->Refresh(items_);
        }
    }

private:
    void RenderTable() {
        auto allColumns = currentController_->GetColumns();

        if (ImGui::BeginTable("data", visibleColumnIndices_.size(),
                             ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable |
                             ImGuiTableFlags_Reorderable)) {

            // Headers
            for (int colIdx : visibleColumnIndices_) {
                ImGui::TableSetupColumn(allColumns[colIdx].displayName.c_str());
            }
            ImGui::TableHeadersRow();

            // Data rows
            for (auto& item : items_) {
                // Apply filter
                if (!PassesFilter(item.get(), allColumns)) continue;

                ImGui::TableNextRow();

                // Track if row is selected
                bool isSelected = IsSelected(item.get());

                for (int colIdx : visibleColumnIndices_) {
                    ImGui::TableSetColumnIndex(colIdx);

                    std::string value = item->GetProperty(allColumns[colIdx].propertyId);

                    // Apply styling based on state
                    if (item->IsDisabled()) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    }

                    ImGui::Selectable(value.c_str(), isSelected,
                                     ImGuiSelectableFlags_SpanAllColumns);

                    if (item->IsDisabled()) {
                        ImGui::PopStyleColor();
                    }

                    // Handle selection
                    if (ImGui::IsItemClicked()) {
                        HandleSelection(item.get());
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    void RenderContextMenu() {
        if (ImGui::BeginPopupContextItem("context_menu")) {
            auto actions = currentController_->GetAvailableActions(selectedItems_);

            for (const auto& action : actions) {
                if (action.isSeparator) {
                    ImGui::Separator();
                } else {
                    if (ImGui::MenuItem(action.displayName.c_str())) {
                        currentController_->ExecuteAction(action.actionId, selectedItems_);

                        // Some actions might need UI follow-up
                        HandleActionResult(action.actionId);
                    }
                }
            }

            ImGui::EndPopup();
        }
    }

    void HandleActionResult(int actionId) {
        // For actions that need UI interaction (dialogs, etc.)
        // This is where UI-specific logic goes
        // Example: Open a dialog for "Change Startup Type"
    }

    bool PassesFilter(DataObject* item, std::span<const DataObjectColumn> columns) const {
        if (filterText_.empty()) return true;

        // Check if any column value contains filter text
        for (const auto& col : columns) {
            std::string value = item->GetProperty(col.propertyId);
            if (value.find(filterText_) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};
```

**Key Separation**:
- **Controller**: Knows WHAT data exists, WHAT actions are possible, HOW to execute actions
- **MainWindow**: Knows HOW to render with ImGui, HOW to present to user, WHICH columns to show

**Result**: Controller can be reused with Qt, WinUI, web, command-line - anything.

### Core Components

#### 1. ViewManager
Lightweight class for managing controller switching.

**Responsibilities:**
- Controller registration
- Tab rendering
- Current controller tracking
- Auto-refresh timer

**Key Operations:**
- `RegisterController(ViewType, unique_ptr<DataController>)`
- `SwitchToView(ViewType)`
- `GetCurrentController()`

#### 2. RefreshManager<T>
Smart data update mechanism for efficient UI updates.

**Template Parameters:**
- `T`: Data model type implementing `IDataModel`

**Responsibilities:**
- Track existing items by ID
- Update existing items without full replacement
- Remove stale items
- Add new items
- Trigger minimal UI updates

**Algorithm:**
1. Mark all existing items as potentially stale
2. Fetch new data from source
3. For each new item:
   - If exists: update in-place and mark as active
   - If new: add to collection
4. Remove all items still marked as stale

#### 3. SettingsManager
Configuration persistence using hierarchical configuration system (see Configuration Management section).

**Responsibilities:**
- Load/save application settings via TomlBackend
- Per-view column configuration (order, width, visibility)
- Window geometry persistence
- Auto-refresh interval
- Remote machine history
- Theme preferences

**Storage Location:**
`%APPDATA%\pserv5\config.toml`

**Access Pattern:**
```cpp
// Use global theSettings instance
using namespace pserv::config;

int interval = theSettings.application.autoRefreshInterval;
theSettings.window.width.set(1280);

// Save changes
TomlBackend backend{configPath};
theSettings.save(backend);
```

Note: Detailed configuration structure is documented in the Configuration Management section.

#### 4. CommandLineProcessor
Handles command-line arguments for automation.

**Supported Arguments:**
- `/SERVICES` - Launch with services view
- `/DEVICES` - Launch with devices view
- `/PROCESSES` - Launch with processes view
- `/WINDOWS` - Launch with windows view
- `/MODULES` - Launch with modules view
- `/UNINSTALLER` - Launch with uninstaller view
- `/DUMPXML <file>` - Export current view to XML and exit
- `/START <service>` - Start service and exit
- `/STOP <service>` - Stop service and exit
- `/RESTART <service>` - Restart service and exit

**Examples:**
```cmd
pserv5.exe /RESTART Apache2
pserv5.exe /SERVICES /DUMPXML services.xml
pserv5.exe /DEVICES /DUMPXML devices.xml
```

## Windows API Abstraction

### UTF-8 Everywhere Strategy

**Internal Code**: All strings are `std::string` (UTF-8)
**Windows API Boundary**: Convert to `std::wstring` only when calling Windows APIs

```cpp
// Utility functions in utils/string_utils.h
namespace pserv5 {

inline std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};

    int size = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                     static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(size, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                         static_cast<int>(utf8.size()), result.data(), size);
    return result;
}

inline std::string WideToUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};

    int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                     static_cast<int>(wide.size()),
                                     nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                         static_cast<int>(wide.size()),
                         result.data(), size, nullptr, nullptr);
    return result;
}

} // namespace pserv5
```

### Service Management Layer

#### ServiceControlManager
RAII wrapper for SCM handle with remote support.

**Constructor:**
```cpp
ServiceControlManager(
    std::string_view machineName = "",  // UTF-8 internally
    DWORD desiredAccess = SC_MANAGER_ALL_ACCESS
);
```

**Implementation Example:**
```cpp
ServiceControlManager::ServiceControlManager(std::string_view machineName, DWORD desiredAccess) {
    // Convert to wstring only at API boundary
    std::wstring wideMachine = machineName.empty() ? L"" : Utf8ToWide(machineName);

    m_hSCM = ::OpenSCManagerW(
        wideMachine.empty() ? nullptr : wideMachine.c_str(),
        nullptr,
        desiredAccess
    );

    if (!m_hSCM) {
        throw std::system_error(::GetLastError(), std::system_category());
    }
}
```

**Key Methods:**
```cpp
expected<std::vector<ServiceInfo>, std::error_code> EnumerateServices();
expected<Service, std::error_code> OpenService(std::string_view name);  // UTF-8
```

#### Service
RAII wrapper for individual service handle.

**Key Methods:**
```cpp
expected<void, std::error_code> Start();
expected<void, std::error_code> Stop();
expected<void, std::error_code> Pause();
expected<void, std::error_code> Continue();
expected<void, std::error_code> Restart();
expected<ServiceStatus, std::error_code> QueryStatus();
expected<ServiceConfig, std::error_code> QueryConfig();
expected<void, std::error_code> ChangeStartType(ServiceStartType type);
expected<void, std::error_code> Delete();
```

**Implementation Example:**
```cpp
expected<void, std::error_code> Service::Start() {
    // Windows API takes no string parameters, just handle
    if (!::StartServiceW(m_hService, 0, nullptr)) {
        return std::unexpected(std::error_code(::GetLastError(), std::system_category()));
    }
    return {};
}
```

#### ServiceInfo
Data model for a service - **all strings are UTF-8**.

**Properties:**
```cpp
std::string name;           // UTF-8
std::string displayName;    // UTF-8
std::string description;    // UTF-8
ServiceStatus status;
ServiceStartType startType;
std::string binaryPath;     // UTF-8
std::string account;        // UTF-8
DWORD processId;
```

**Key Point**: When fetching from Windows APIs, convert immediately:
```cpp
// In ServiceManager::EnumerateServices()
auto wideName = serviceStatus.lpServiceName;
auto wideDisplayName = serviceStatus.lpDisplayName;

ServiceInfo info{};
info.name = WideToUtf8(wideName);           // Convert at boundary
info.displayName = WideToUtf8(wideDisplayName);
// ... rest of internal code uses UTF-8
```

### Error Handling Strategy

Use `expected<T, std::error_code>` pattern (similar to C++23 std::expected):

```cpp
auto result = service.Start();
if (result) {
    spdlog::info("Service started successfully");
} else {
    spdlog::error("Failed to start service: {}", result.error().message());
    // Display error in UI
}
```

### Resource Management

All Windows handles wrapped with wil RAII types:
- `wil::unique_schandle` - Service Control Manager and Service handles
- `wil::unique_handle` - Generic handles
- `wil::unique_hlocal` - Local memory allocations
- `wil::unique_process_heap_ptr<T>` - Process heap allocations

**Example:**
```cpp
wil::unique_schandle hSCM(
    ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE)
);
if (!hSCM) {
    return std::unexpected(std::error_code(::GetLastError(), std::system_category()));
}
// Automatic cleanup when hSCM goes out of scope
```

## Completed Milestones (Historical Reference)

**Milestones 0-27 have been completed** and are documented in git commit history. Key commits:
- `6b7ebbb` - "Completely redesigned controller integration for clean separation"
- `6f34da6` - "Add Devices view with full UI implementation"
- `def5a3a` - "Implement Milestone 27: Title bar styling and polish"
- `acace08` - "Implement Milestone 26: Modern menu bar with ImGui"

For detailed implementation history, see git log and `.claude_notes.md`.

---

## Installation Strategy

**Decision**: WiX Toolset (MSI-based installer)

**Rationale:**
1. pserv is a system administration tool → MSI is expected in enterprise environments
2. Supports major upgrades cleanly (important for long-term maintenance)
3. Good Visual Studio integration
4. Professional appearance and industry standard

**Implementation**: See Phase 10 (Polish & Beta Release) for detailed WiX installer deliverables and acceptance criteria.

**Installer Structure:**
```
pserv5_installer/
├── Product.wxs          # Main installer definition
├── Components.wxs       # File components
└── UI.wxs               # Custom dialogs (optional)
```

**Key Features:**
- MSI-based with proper versioning and upgrade support
- Install to `%ProgramFiles%\pserv5\`
- Create Start Menu shortcut
- Register in Add/Remove Programs
- Create `%APPDATA%\pserv5\` directory on first run
- Support silent install: `msiexec /i pserv5-setup-5.0.0.msi /quiet`
- Optional: Code signing for release builds (recommended for enterprise distribution)

## High-Level Implementation Phases

### Phase 1: Foundation & Infrastructure

#### Deliverables:
1. Basic ImGui application with Win32 window and DirectX 11 rendering
2. spdlog integration with rotating file sink
3. Core interfaces defined (`IViewController`, `IDataModel`, etc.)
4. ViewManager implementation
5. SettingsManager with hierarchical configuration system (Config classes)
6. RefreshManager template implementation

#### Acceptance Criteria:
- Application launches and displays ImGui window
- Logging works and writes to file
- Settings can be saved/loaded using hierarchical configuration system
- Empty tab bar renders with placeholder views

#### Estimated Effort: 2 weeks

### Phase 2: Windows API Wrappers

#### Deliverables:
1. WIL integration (add as submodule)
2. ServiceControlManager class with remote support
3. Service class with all operations
4. ProcessManager class
5. WindowManager class
6. RegistryAccessor class
7. Error handling utilities

#### Acceptance Criteria:
- Can enumerate services locally and remotely
- Can perform all service operations
- All Windows API errors mapped to error_code
- Unit tests for each manager class

#### Estimated Effort: 1-2 weeks

### Phase 3: Services View (Core Feature)

#### Deliverables:
1. ServiceInfo model
2. ServicesDataController
3. ServicesViewController with ImGui rendering
4. Context menu with all operations
5. Filtering and sorting
6. Multi-column sort support
7. Status icons rendering
8. Template import/export for services

#### Features:
- List all services with columns: Name, Display Name, Status, Start Type, Binary Path, Description, Process ID
- Context menu operations:
  - Start / Stop / Pause / Continue / Restart
  - Change Startup Type (Automatic / Manual / Disabled)
  - Uninstall Service
  - Open Registry Editor at service key
  - Start CMD in service binary path
  - Copy service information
- Real-time status updates (5-second refresh)
- Column reordering and resizing
- Multi-column sorting (Ctrl+Click)
- Filter by any column content
- Export to XML
- Import/apply templates

#### Acceptance Criteria:
- All pserv4 service features working
- Remote machine connectivity functional
- Performance equal to or better than pserv4
- No memory leaks

#### Estimated Effort: 2-3 weeks

### Phase 4: Devices View

#### Deliverables:
1. Inherit from ServicesDataController (filter for drivers)
2. DevicesViewController
3. Device-specific context menu items

#### Acceptance Criteria:
- Shows only kernel and file system drivers
- All service operations work on drivers
- Filter and sort functional

#### Estimated Effort: 3-5 days

### Phase 5: Processes View

#### Deliverables:
1. ProcessInfo model
2. ProcessesDataController
3. ProcessesViewController
4. Process operations (terminate, change priority)
5. Performance metrics display

#### Features:
- Columns: Name, PID, Memory Usage, CPU %, User, Path, Command Line
- Context menu: Terminate, Change Priority, Open File Location
- Real-time performance updates

#### Acceptance Criteria:
- All pserv4 process features working
- Performance metrics accurate
- Can terminate processes

#### Estimated Effort: 1 week

### Phase 6: Windows View

#### Deliverables:
1. WindowInfo model
2. WindowsDataController
3. WindowsViewController
4. Window manipulation operations

#### Features:
- Columns: Title, Class, Process, Visible, Handle
- Context menu: Show, Hide, Minimize, Maximize, Restore, Bring to Front, Close
- Real-time window list updates

#### Acceptance Criteria:
- All pserv4 window features working
- Can manipulate windows correctly

#### Estimated Effort: 1 week

### Phase 7: Modules View

#### Deliverables:
1. ModuleInfo model
2. ModulesDataController
3. ModulesViewController

#### Features:
- Columns: Module Name, Path, Process, Version, Size
- Context menu: Open File Location, Copy Path
- Filter by module name or process

#### Acceptance Criteria:
- Lists all loaded modules across all processes
- Can identify DLL path conflicts

#### Estimated Effort: 3-5 days

### Phase 8: Uninstaller View

#### Deliverables:
1. InstalledProgramInfo model
2. UninstallerDataController (reads registry)
3. UninstallerViewController

#### Features:
- Columns: Name, Publisher, Version, Install Date, Size, Uninstall Command
- Context menu: Uninstall, Open Install Location, Open Registry Key
- Search and filter

#### Acceptance Criteria:
- Lists all installed programs from registry
- Can launch uninstall commands
- Shows program details

#### Estimated Effort: 1 week

### Phase 9: Command-Line Interface

#### Deliverables:
1. CommandLineProcessor implementation
2. Headless mode for automation
3. XML export for all views
4. Service start/stop/restart commands

#### Acceptance Criteria:
- All command-line switches work
- Can automate service operations
- XML export is well-formed and compatible with pserv4 format

#### Estimated Effort: 3-5 days

### Phase 10: Polish & Beta Release

#### Deliverables:
1. Comprehensive testing (all views, all operations)
2. Performance optimization and profiling
3. Memory leak detection and fixing (valgrind/Application Verifier)
4. Icon and resource integration (application icon, version info)
5. User documentation (README, help system)
6. **WiX Installer project**:
   - Create `pserv5_installer/` project in solution
   - MSI-based installer with proper versioning
   - Install to `%ProgramFiles%\pserv5\`
   - Create Start Menu shortcut
   - Register in Add/Remove Programs
   - Create `%APPDATA%\pserv5\` directory on first run
   - Support silent install: `msiexec /i pserv5-setup-5.0.0.msi /quiet`
   - Support upgrade path from future versions

#### Acceptance Criteria:
- No known critical bugs
- No memory leaks detected
- Performance meets or exceeds pserv4
- Documentation complete
- **Installer works on clean Windows 10/11 system**
- **Uninstaller removes all files cleanly**
- **Major upgrade support tested**

#### Beta Release Milestone:
- **Version**: 5.0.0-beta1
- **Installer**: `pserv5-setup-5.0.0-beta1.msi`
- **Release Notes**: Document known issues and test feedback channels

#### Estimated Effort: 1-2 weeks

## Project Structure

```
pserv5/
├── pserv5/                    # Main project directory
│   ├── pserv5.cpp             # Entry point: WinMain, Win32 window creation
│   ├── pserv5.h               # Main header
│   ├── main_window.cpp/h      # Main window class with ImGui loop
│   ├── precomp.cpp/h          # Precompiled header
│   ├── pserv5.slnx            # Visual Studio 2022 Solution (XML format)
│   ├── pserv5.vcxproj         # Project file
│   ├── pserv5.manifest        # Application manifest
│   ├── pserv5.rc              # Resource file
│   ├── Resource.h             # Resource IDs
│   │
│   ├── core/                  # Core abstractions
│   │   ├── data_object.h          # DataObject base class
│   │   ├── data_controller.h/cpp  # DataController base class
│   │   ├── data_object_column.h   # Column descriptor
│   │   ├── async_operation.h/cpp  # Background task management
│   │   ├── IRefCounted.h          # Reference counting interface
│   │   └── data_controller_library.h/cpp # Library of controllers
│   │
│   ├── Config/               # Configuration system
│   │   ├── config_backend.h       # Abstract backend interface
│   │   ├── value_interface.h      # Base for all config items
│   │   ├── section.h/cpp          # Hierarchical sections
│   │   ├── typed_value.h          # TypedValue<T> template
│   │   ├── typed_vector_value.h   # TypedValueVector<T> template
│   │   ├── toml_backend.h         # TOML storage backend
│   │   └── settings.h/cpp         # Global theSettings instance
│   │
│   ├── windows_api/       # Windows API wrappers (data sources)
│   │   ├── service_manager.cpp/h
│   │   ├── process_manager.cpp/h
│   │   ├── window_manager.cpp/h   # (Planned)
│   │   ├── registry_accessor.cpp/h # (Planned)
│   │   └── module_enumerator.cpp/h # (Planned)
│   │
│   ├── models/            # DataObject implementations
│   │   ├── service_info.cpp/h
│   │   ├── process_info.cpp/h
│   │   ├── window_info.cpp/h      # (Planned)
│   │   ├── module_info.cpp/h      # (Planned)
│   │   └── installed_program_info.cpp/h # (Planned)
│   │
│   ├── controllers/       # DataController implementations
│   │   ├── services_data_controller.cpp/h
│   │   ├── devices_data_controller.h
│   │   ├── processes_data_controller.cpp/h
│   │   ├── windows_data_controller.cpp/h # (Planned)
│   │   ├── modules_data_controller.cpp/h # (Planned)
│   │   └── uninstaller_data_controller.cpp/h # (Planned)
│   │
│   ├── dialogs/           # UI Dialogs
│   │   ├── service_properties_dialog.cpp/h
│   │   └── process_properties_dialog.cpp/h
│   │
│   └── utils/             # Utilities
│       ├── logging.cpp/h      # spdlog initialization
│       ├── win32_error.h      # Windows error codes
│       └── string_utils.h     # Utf8ToWide/WideToUtf8 conversion
│
├── imgui/                     # Git submodule
├── spdlog/                    # Git submodule
├── tomlplusplus/              # Git submodule
├── rapidjson/                 # Git submodule
├── wil/                       # Git submodule
├── pugixml/                   # Git submodule (To be added)
│
├── archive/                   # pserv4 reference implementation
│   ├── pserv4/                # C# WPF source code
│   └── pserv.cpl 4.1.html     # Website documentation
│
├── instructions.md            # This document
├── GEMINI.md                  # Agent context and memory
└── README.md                  # User-facing documentation
```

**Key Points**:
- **NO separate "views" directory**: Controllers handle rendering via `RenderTable()` and `RenderContextMenu()`
- **models/**: One file per DataObject subclass (pure data + property accessors)
- **controllers/**: One file per DataController subclass (logic + rendering + actions)
- **windows_api/**: Pure Windows API wrappers (no UI code)

## Configuration Management

### Hierarchical Configuration System

This project uses a custom hierarchical configuration system ported from the jucyaudio project. The system provides type-safe, hierarchical settings with automatic serialization/deserialization.

#### Architecture Overview

The configuration system consists of four main components:

1. **ConfigBackend** - Abstract interface for storage backends (TOML, INI, Registry, etc.)
2. **ValueInterface** - Common interface for all configuration items
3. **Section** - Groups configuration items hierarchically
4. **TypedValue<T>** - Strongly-typed leaf values
5. **TypedValueVector<T>** - Strongly-typed collections of sections

#### Source Files (Copy from archive\Config\)

**Core Classes:**
- `config_backend.h` - Abstract backend interface with IEnumConfigValue support
- `value_interface.h` - Base interface for all configuration items
- `section.h` / `section.cpp` - Hierarchical grouping container
- `typed_value.h` - Template for individual typed settings
- `typed_vector_value.h` - Template for collections of sections

**Backend Implementation:**
- `toml_backend.h` - TOML file backend using toml++ library

**Usage Example (Settings.h/.cpp):**
- `Settings.h` - RootSettings structure showing best practices
- `Settings.cpp` - Global `theSettings` instance and initialization

#### Integration Steps

1. Copy all files from `archive\Config\` to `pserv5\Config\`
2. Change namespace from `jucyaudio::config` to `pserv::config`
3. Update includes to use `#include <Config/...>` pattern
4. Add `Config\` directory to Visual Studio project
5. Link against toml++ (already present as submodule)

#### Settings Structure for pserv5

**Location:** `%APPDATA%\pserv5\config.toml` (handled by TomlBackend)

**Structure Definition (pserv5\Config\Settings.h):**

```cpp
#pragma once

#include <Config/section.h>
#include <Config/typed_value.h>
#include <Config/typed_vector_value.h>

namespace pserv::config
{
    extern std::shared_ptr<spdlog::logger> logger;

    // Column configuration for each view
    struct ViewColumnSection : public Section
    {
        ViewColumnSection(Section* m_pParent, const std::string& name)
            : Section{m_pParent, name}
        {}

        TypedValue<std::string> columnName{this, "ColumnName", ""};
        TypedValue<int> columnWidth{this, "ColumnWidth", 100};
        TypedValue<bool> visible{this, "Visible", true};
    };

    class RootSettings : public Section
    {
    public:
        RootSettings() : Section{} {}

        // Application Settings
        struct ApplicationSettings : public Section
        {
            ApplicationSettings(Section* m_pParent)
                : Section{m_pParent, "Application"}
            {}

            TypedValue<std::string> version{this, "Version", "5.0.0"};
            TypedValue<bool> m_bAutoRefreshEnabled{this, "AutoRefreshEnabled", true};
            TypedValue<int> autoRefreshInterval{this, "AutoRefreshInterval", 5};
            TypedValue<std::string> theme{this, "Theme", "dark"};
            TypedValue<std::string> logLevel{this, "LogLevel", "info"};

        } application{this};

        // Window Settings
        struct WindowSettings : public Section
        {
            WindowSettings(Section* m_pParent)
                : Section{m_pParent, "Window"}
            {}

            TypedValue<int> width{this, "Width", 1280};
            TypedValue<int> height{this, "Height", 720};
            TypedValue<bool> m_bMaximized{this, "Maximized", false};
            TypedValue<int> positionX{this, "PositionX", 100};
            TypedValue<int> positionY{this, "PositionY", 100};

        } window{this};

        // Services View Settings
        struct ServicesViewSettings : public Section
        {
            ServicesViewSettings(Section* m_pParent)
                : Section{m_pParent, "ServicesView"}
            {}

            TypedValueVector<ViewColumnSection> columns{this, "Columns"};
            TypedValue<std::string> sortColumn{this, "SortColumn", "Name"};
            TypedValue<bool> m_bSortAscending{this, "SortAscending", true};
            TypedValue<std::string> filterText{this, "FilterText", ""};

        } servicesView{this};

        // Devices View Settings
        struct DevicesViewSettings : public Section
        {
            DevicesViewSettings(Section* m_pParent)
                : Section{m_pParent, "DevicesView"}
            {}

            TypedValueVector<ViewColumnSection> columns{this, "Columns"};
            TypedValue<std::string> sortColumn{this, "SortColumn", "Name"};
            TypedValue<bool> m_bSortAscending{this, "SortAscending", true};
            TypedValue<std::string> filterText{this, "FilterText", ""};

        } devicesView{this};

        // Processes View Settings
        struct ProcessesViewSettings : public Section
        {
            ProcessesViewSettings(Section* m_pParent)
                : Section{m_pParent, "ProcessesView"}
            {}

            TypedValueVector<ViewColumnSection> columns{this, "Columns"};
            TypedValue<std::string> sortColumn{this, "SortColumn", "Name"};
            TypedValue<bool> m_bSortAscending{this, "SortAscending", true};
            TypedValue<std::string> filterText{this, "FilterText", ""};

        } processesView{this};

        // Windows View Settings
        struct WindowsViewSettings : public Section
        {
            WindowsViewSettings(Section* m_pParent)
                : Section{m_pParent, "WindowsView"}
            {}

            TypedValueVector<ViewColumnSection> columns{this, "Columns"};
            TypedValue<std::string> sortColumn{this, "SortColumn", "Title"};
            TypedValue<bool> m_bSortAscending{this, "SortAscending", true};
            TypedValue<std::string> filterText{this, "FilterText", ""};

        } windowsView{this};

        // Modules View Settings
        struct ModulesViewSettings : public Section
        {
            ModulesViewSettings(Section* m_pParent)
                : Section{m_pParent, "ModulesView"}
            {}

            TypedValueVector<ViewColumnSection> columns{this, "Columns"};
            TypedValue<std::string> sortColumn{this, "SortColumn", "Name"};
            TypedValue<bool> m_bSortAscending{this, "SortAscending", true};
            TypedValue<std::string> filterText{this, "FilterText", ""};

        } modulesView{this};

        // Uninstaller View Settings
        struct UninstallerViewSettings : public Section
        {
            UninstallerViewSettings(Section* m_pParent)
                : Section{m_pParent, "UninstallerView"}
            {}

            TypedValueVector<ViewColumnSection> columns{this, "Columns"};
            TypedValue<std::string> sortColumn{this, "SortColumn", "DisplayName"};
            TypedValue<bool> m_bSortAscending{this, "SortAscending", true};
            TypedValue<std::string> filterText{this, "FilterText", ""};

        } uninstallerView{this};

        // Remote Connection Settings
        struct RemoteSettings : public Section
        {
            RemoteSettings(Section* m_pParent)
                : Section{m_pParent, "Remote"}
            {}

            TypedValue<bool> m_bEnabled{this, "Enabled", true};
            TypedValue<std::string> lastMachine{this, "LastMachine", "localhost"};
            TypedValue<int> timeoutSeconds{this, "TimeoutSeconds", 30};
            // Note: Recent machines stored as TypedValueVector if needed

        } remote{this};

        // Template Settings
        struct TemplateSettings : public Section
        {
            TemplateSettings(Section* m_pParent)
                : Section{m_pParent, "Templates"}
            {}

            TypedValue<std::string> lastImportPath{this, "LastImportPath", ""};
            TypedValue<std::string> lastExportPath{this, "LastExportPath", ""};

        } templates{this};

        // Advanced Settings
        struct AdvancedSettings : public Section
        {
            AdvancedSettings(Section* m_pParent)
                : Section{m_pParent, "Advanced"}
            {}

            TypedValue<bool> m_bEnableUnsafeOperations{this, "EnableUnsafeOperations", false};
            TypedValue<bool> m_bConfirmDestructiveActions{this, "ConfirmDestructiveActions", true};
            TypedValue<bool> m_bShowSystemServices{this, "ShowSystemServices", true};
            TypedValue<bool> m_bShowHiddenProcesses{this, "ShowHiddenProcesses", true};

        } advanced{this};
    };

    // Global settings instance
    extern RootSettings theSettings;

} // namespace pserv::config
```

#### Usage Example:

```cpp
#include <Config/Settings.h>
#include <Config/toml_backend.h>

using namespace pserv::config;

// Initialization (typically in WinMain)
void InitializeSettings()
{
    // Get config file path
    std::string configPath = GetAppDataPath() + "\\pserv5\\config.toml";

    // Create backend
    TomlBackend backend{configPath};

    // Load settings (populates theSettings with saved values or defaults)
    theSettings.load(backend);
}

// Accessing settings
void UseSettings()
{
    // Read values (implicit conversion operators)
    int interval = theSettings.application.autoRefreshInterval;
    std::string theme = theSettings.application.theme;
    bool m_bMaximized = theSettings.window.m_bMaximized;

    // Or use explicit get()
    int width = theSettings.window.width.get();

    // Modify values (using assignment operator overload)
    theSettings.application.theme = "light";
    theSettings.window.width = 1920;

    // Save back to file
    TomlBackend backend{configPath};
    theSettings.save(backend);
}

// Working with column configurations
void ConfigureColumns()
{
    auto& columns = theSettings.servicesView.columns;

    // Clear existing
    columns.clear();

    // Add new columns
    auto* m_pCol = columns.addNew();
    m_pCol->columnName = "Name";
    m_pCol->columnWidth = 150;
    m_pCol->visible = true;

    // Access existing columns
    for (size_t i = 0; i < columns.size(); ++i)
    {
        auto* m_pCol = columns[i];
        spdlog::info("Column: {}, Width: {}",
                     m_pCol->columnName.get(),
                     m_pCol->columnWidth.get());
    }
}
```

#### Key Features

1. **Type Safety**: Compile-time type checking for all settings
2. **Hierarchical Organization**: Natural nesting with Section classes
3. **Default Values**: Each TypedValue has a default (used on first load)
4. **Automatic Serialization**: Backend handles all I/O
5. **Collections**: TypedValueVector for dynamic lists of settings
6. **Path-Based Access**: Settings use filesystem-like paths (`Application/Theme`)
7. **Backend Abstraction**: Easy to swap TOML for Registry, INI, JSON, etc.
8. **Revert to Defaults**: Built-in support via `revertToDefault()`

#### Backend Interface

The `ConfigBackend` abstract class supports:
- `load(path, value)` / `save(path, value)` for int, bool, string
- `sectionExists(path)` - Check if a section exists
- `deleteKey(path)` / `deleteSection(path)` - Remove settings
- Automatic IEnumConfigValue support for custom enums

Current implementation: `TomlBackend` using toml++ library

## Visual Studio Project Configuration

### Additional Include Directories
```
$(SolutionDir)..\imgui
$(SolutionDir)..\imgui\backends
$(SolutionDir)..\spdlog\include
$(SolutionDir)..\tomlplusplus\include
$(SolutionDir)..\wil\include
$(SolutionDir)..\pugixml\src
```

### Preprocessor Definitions
```
SPDLOG_COMPILED_LIB
UNICODE
_UNICODE
WIN32_LEAN_AND_MEAN
NOMINMAX
```

### Additional Dependencies (Linker)
```
d3d11.lib
d3dcompiler.lib
dxgi.lib
advapi32.lib
user32.lib
gdi32.lib
shell32.lib
```

### Language Standard
```
C++20 (/std:c++20)
```

### Conformance Mode
```
Yes (/permissive-)
```

## Testing Strategy

### Unit Tests
- Use Catch2 or Google Test framework
- Mock Windows APIs where possible
- Test all manager classes independently
- Test data model operations
- Test refresh logic
- Test error handling paths

### Integration Tests
- Test actual Windows API calls on local machine
- Test remote connectivity (if available)
- Test XML import/export roundtrip
- Test command-line interface
- Test settings persistence

### Performance Tests
- Benchmark service enumeration (should be < 100ms for 200 services)
- Benchmark refresh cycle (should be < 50ms)
- Memory leak detection (use Visual Studio diagnostics)
- Stress test with many services/processes

### UI Tests
- Manual testing of all views
- Verify all context menu operations
- Test filtering and sorting
- Test column reordering
- Test theme switching

## Success Criteria

### Functional Requirements
- [ ] All 6 views implemented with full feature parity to pserv4
- [ ] Remote machine connectivity works
- [ ] Command-line interface supports all operations
- [ ] XML export/import compatible with pserv4 format
- [ ] Settings persist correctly across sessions
- [ ] All service operations (start/stop/pause/continue/restart) work
- [ ] All window operations work
- [ ] Process management works
- [ ] Uninstaller displays all programs correctly
- [ ] Modules view shows all loaded DLLs

### Non-Functional Requirements
- [ ] Performance equal to or better than pserv4
- [ ] No memory leaks
- [ ] Binary size < 5 MB
- [ ] Startup time < 1 second
- [ ] No runtime dependencies (statically linked)
- [ ] Works on Windows 7, 8, 10, 11
- [ ] Supports High DPI displays
- [ ] Comprehensive logging for troubleshooting

## Risk Assessment

### High Risks
1. **Remote connectivity complexity**: Requires careful testing across network scenarios
   - Mitigation: Implement early, test incrementally, provide good error messages

2. **Windows API edge cases**: Many undocumented behaviors
   - Mitigation: Reference pserv4 implementation, thorough testing

3. **ImGui learning curve**: Team unfamiliar with immediate-mode GUI
   - Mitigation: Start with simple views, refer to examples, iterate

### Medium Risks
1. **Performance regression**: Native code should be faster, but poor algorithms could slow it down
   - Mitigation: Profile early and often, benchmark against pserv4

2. **WIL integration issues**: New dependency
   - Mitigation: Validate WIL with simple test cases first

### Low Risks
1. **tomlplusplus issues**: Well-established library
   - Mitigation: Use straightforward TOML structures

2. **Build system complexity**: Using familiar Visual Studio tooling
   - Mitigation: Standard MSBuild, no exotic configuration

## Critical Architectural Decisions (Summary)

### ✅ REFINED: UI-Independent DataController Pattern

After reviewing pserv4's architecture and discussing improvements, we've refined the **DataController/DataObject/DataObjectColumn** pattern for better separation of concerns.

**Evolution from pserv4**:

| pserv4 (C# WPF) | pserv5 (C++ ImGui) | Why Changed |
|-----------------|-------------------|-------------|
| Controller contains WPF code | Controller is UI-agnostic | Enable porting to other UIs |
| Controller builds ContextMenu objects | Controller returns Action list | Decouple from UI framework |
| String-based property access | int-based with enums | Performance + type safety |
| Controller manages column order | UI layer manages column order | Column order is UI state |
| Mixed responsibilities | Clean separation | Better testability |

**Core Pattern (Refined for pserv5)**:

1. **DataObject**: Pure data model with int-based property access
   - Each controller defines its own `PropertyEnum` (ServiceProperty, ProcessProperty, etc.)
   - `GetProperty(int)` uses switch statement for performance
   - Generic consumers (UI, XML) work with `int` - don't need to know types

2. **DataController**: UI-independent business logic
   - Provides available columns (not order - that's UI state)
   - Returns available actions based on selection
   - Executes actions when requested
   - Contains ZERO UI framework code (no ImGui, no WPF, nothing)

3. **Action**: Like DataObjectColumn, but for operations
   - Each controller defines its own `ActionEnum`
   - Actions returned as opaque `int` to UI
   - UI renders generically, controller decides validity

4. **MainWindow (UI Layer)**: All ImGui code lives here
   - Generically renders any controller
   - Manages UI state (column order, selection, filter)
   - Completely generic - works with any controller

**Key Benefits**:
1. **Portability**: Controllers can be reused with Qt, WinUI, web, CLI
2. **Testability**: Controllers can be unit tested without UI
3. **Simplicity**: Still one controller per view, but better organized
4. **Performance**: int-based lookups with enums
5. **Type Safety**: Enums prevent typos within each controller

**Implementation Rule**:
> When you add a new view, you create:
> 1. PropertyEnum and ActionEnum (controller-specific)
> 2. DataObject subclass (uses PropertyEnum)
> 3. DataController subclass (uses both enums, provides everything)
>
> UI layer automatically works with it - no changes needed.

## Open Questions

1. **Icon Resources**: Should we reuse pserv4 icons or create new ones?
2. **Installer**: Use WiX, NSIS, or Inno Setup?
3. **Versioning**: Start at 5.0.0 or 5.0.0.0 to match pserv4 scheme?
4. **License**: Continue BSD license from pserv4?
5. **Code Signing**: Do we need to sign the executable?
6. **Backwards Compatibility**: Should XML templates from pserv4 work in pserv5?
7. **Unit Testing Framework**: Catch2, Google Test, or Doctest?
8. **CI/CD**: GitHub Actions for automated builds?

## Next Steps

1. **Review this document** - Gather feedback and refine plan
2. **Add remaining submodules** - WIL and pugixml
3. **Update Visual Studio project** - Add all include paths and dependencies
4. **Create project structure** - Set up folder hierarchy
5. **Implement Phase 1** - Foundation and infrastructure
6. **Iterative development** - Build and test each phase incrementally

## References

- pserv4 source code: `C:\Projects\2025\11\pserv5\archive\pserv4\`
- pserv website: http://p-nand-q.com/download/pserv_cpl/index.html
- ImGui documentation: https://github.com/ocornut/imgui
- spdlog documentation: https://github.com/gabime/spdlog
- tomlplusplus documentation: https://github.com/marzer/tomlplusplus
- WIL documentation: https://github.com/microsoft/wil
- pugixml documentation: https://pugixml.org/

---

**Document Version**: 4.0
**Date**: 2025-11-15
**Author**: Engineering Team
**Status**: Draft - Pending Final Review

**Major Changes in v4.0** (Concurrency & Implementation Details):
- **COM-Style Refcounting**: Explicit retain/release with debugging macros, thread-safe via InterlockedIncrement/Decrement
- **Asynchronous Operations**: std::async with Windows message queue for UI communication, progress reporting, cancellation support
- **MFC Naming Conventions**: m_ prefix for members, m_b for booleans, m_p for pointers
- **Curly Brace Initialization**: All member variables use {} initialization
- **UTF-8 Everywhere**: std::string internally, convert to std::wstring only at Windows API boundaries
- **Error Reporting**: Controllers return bool + GetLastErrorMessage() pattern
- **One Operation at a Time**: Modal progress dialog with cancel button

**Major Changes in v3.0** (Engineering Refinements):
- **UI Independence**: Controllers now contain ZERO UI framework code
- **Action System**: Introduced Action abstraction with int-based IDs (like columns)
- **Separation of Concerns**: Column ordering moved to UI layer (it's UI state, not data)
- **int-based Properties**: Changed from string to enum-based property IDs for performance
- **Generic UI Layer**: MainWindow renders any controller the same way
- **Portability**: Controllers can now be reused with any UI framework (Qt, WinUI, web, CLI)

**Major Changes in v2.0**:
- Clarified DataController/DataObject/DataObjectColumn pattern from pserv4
- Removed incorrect separation of ViewController and DataController
- Added concrete implementation examples
- Updated project structure

---

## UI Modernization Milestones (COMPLETED)

UI modernization (Milestones 24-27) has been **completed**. The application now features:
- Borderless window with custom title bar
- Windows 11 accent color integration
- Modern ImGui menu bar
- Custom window controls (minimize, maximize, close)
- DPI-aware rendering

See git commits `def5a3a` through `acace08` for implementation details.

