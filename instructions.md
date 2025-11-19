# pserv5 Development Reference

## Project Status

### Completed (Phases 1-6)
- ✅ Foundation: ImGui + DirectX 11, spdlog logging, TOML configuration, tab infrastructure
- ✅ Windows API wrappers: ServiceManager, ProcessManager, WindowManager, ModuleManager, UninstallerManager with WIL error handling
- ✅ Services View: Complete service management with properties dialog, filtering, sorting, async operations
- ✅ Devices View: Driver management (inherits from ServicesDataController)
- ✅ Processes View: Process enumeration, termination, priority control
- ✅ Windows View: Desktop window enumeration and manipulation
- ✅ UI: Custom title bar, modern menu bar, DPI-aware rendering

### In Progress
- 🏃 Modules View: NEEDS REDESIGN - currently enumerates all modules globally (performance issue)
- 🏃 Uninstaller View: Bug in refresh timing (line 156), naive uninstall string parsing

### Remaining
- ⬜ Command-Line Interface: Headless mode, XML export/import, automation
- ⬜ Polish & Beta: Testing, memory leak detection, WiX MSI installer

**Current completion: ~75%**

## Core Architecture

### Design Principles
1. **UI Independence**: Controllers contain zero UI framework code - enables future UI ports
2. **Separation of Concerns**: Data (models) / Logic (controllers) / Presentation (UI)
3. **Manual Memory Management**: Raw pointers with `new`/`delete` - explicit ownership
4. **UTF-8 Everywhere**: Convert to UTF-16 only at Windows API boundaries
5. **MFC Conventions**: `m_` prefix for members, `m_b` for booleans, `m_p` for pointers

### Three-Part Pattern (from pserv4, evolved)

**DataObject (Model)**
- Base class for ServiceInfo, ProcessInfo, WindowInfo, etc.
- `GetId()`, `GetProperty(int)`, `Update()`
- Visual state flags: `IsRunning()`, `IsDisabled()`
- See: `core/data_object.h`, `models/service_info.h`

**DataObjectColumn (Metadata)**
- Pairs display name with property identifier
- Controllers define available columns, UI decides which to show
- See: `core/data_object_column.h`

**DataController (Business Logic)**
- UI-independent controller per view type
- Provides: `Refresh()`, `GetColumns()`, `GetAvailableActions()`, `DispatchAction()`
- Owns data objects, manages their lifecycle
- See: `core/data_controller.h`, `controllers/services_data_controller.cpp`

### Memory Management Rules

Controllers own their data objects:
```cpp
// In controller:
std::vector<ServiceInfo*> m_services;

void Refresh() {
    Clear();  // Delete old objects
    m_services = ServiceManager::EnumerateServices();  // Ownership transferred
}

void Clear() {
    for (auto* obj : m_services) delete obj;
    m_services.clear();
}

~Controller() { Clear(); }
```

UI borrows pointers - no ownership:
```cpp
// UI just renders, doesn't delete:
const auto& objects = controller->GetDataObjects();
for (auto* obj : objects) {
    RenderRow(obj);
}
```

### Async Operations

Long-running operations use `AsyncOperation` class:
- Work happens on background thread via lambda
- Progress reported via `ReportProgress(float, string)`
- UI shows modal progress dialog
- Controller refreshes on completion
- See: `core/async_operation.h`, `controllers/services_data_controller.cpp:300-336`

Example pattern:
```cpp
context.m_pAsyncOp = new AsyncOperation();
context.m_bShowProgressDialog = true;

context.m_pAsyncOp->Start(context.m_hWnd, [serviceNames](AsyncOperation* op) -> bool {
    for (size_t i = 0; i < total; ++i) {
        op->ReportProgress((float)i / total, "Starting service...");
        ServiceManager::StartServiceByName(serviceNames[i]);
    }
    return true;
});
```

## Current Implementation Notes

### Known Issues (from Gemini's code review)

**Modules Controller** (`controllers/modules_data_controller.cpp:29-46`)
- ❌ Enumerates ALL modules from ALL processes on every refresh
- ❌ Extremely slow with thousands of objects
- ❌ Should lazy-load modules for selected process only
- ✅ Follow windows controller pattern for parent-child relationships

**Uninstaller Controller** (`controllers/uninstaller_data_controller.cpp`)
- ❌ Line 156: Calls `Refresh()` immediately after launching uninstaller - wrong timing
- ❌ Lines 126-140: Uninstall string parsing doesn't handle quotes properly
- ❌ Line 188: Estimated Size sorting is string-based, not numeric

**Processes Controller** (`controllers/processes_data_controller.cpp`)
- ⚠️ Line 28: `reinterpret_cast<const vector<DataObject*>&>` is undefined behavior
- ⚠️ Should use `m_dataObjects` pattern like other controllers
- ⚠️ Lines 24-26: Lazy loading in const getter is questionable
- ⚠️ Lines 45-51: Username caching never invalidates

### Controller Comparison

ServicesDataController (reference implementation):
- ✅ Proper memory management with Clear()
- ✅ Sort persistence with m_lastSortColumn/m_lastSortAscending
- ✅ Async operations with progress reporting
- ✅ Error handling with user feedback
- ✅ Type-safe enums for properties and actions
- See: `controllers/services_data_controller.cpp`

## Project Structure

```
pserv5/
├── core/                     # Base abstractions
│   ├── data_controller.h/.cpp
│   ├── data_object.h
│   ├── data_object_column.h
│   ├── async_operation.h/.cpp
│   └── data_controller_library.h/.cpp
│
├── models/                   # Data objects
│   ├── service_info.h/.cpp
│   ├── process_info.h/.cpp
│   ├── window_info.h/.cpp
│   ├── module_info.h/.cpp
│   └── installed_program_info.h/.cpp
│
├── controllers/              # Business logic
│   ├── services_data_controller.h/.cpp
│   ├── devices_data_controller.h
│   ├── processes_data_controller.h/.cpp
│   ├── windows_data_controller.h/.cpp
│   ├── modules_data_controller.h/.cpp  (NEEDS REDESIGN)
│   └── uninstaller_data_controller.h/.cpp (HAS BUGS)
│
├── windows_api/              # Windows API wrappers
│   ├── service_manager.h/.cpp
│   ├── process_manager.h/.cpp
│   ├── window_manager.h/.cpp
│   ├── module_manager.h/.cpp
│   └── uninstaller_manager.h/.cpp
│
├── dialogs/                  # Properties dialogs
│   ├── service_properties_dialog.h/.cpp
│   ├── process_properties_dialog.h/.cpp
│   ├── window_properties_dialog.h/.cpp
│   └── uninstaller_properties_dialog.h/.cpp
│
├── utils/                    # Utilities
│   ├── string_utils.h        # Utf8ToWide/WideToUtf8
│   ├── win32_error.h         # GetLastWin32ErrorMessage()
│   └── logging.h/.cpp        # spdlog initialization
│
├── config/                   # Configuration system
│   ├── settings.h/.cpp       # Global theSettings
│   ├── section.h/.cpp
│   ├── typed_value.h
│   └── toml_backend.h
│
├── main_window.h/.cpp        # UI layer (ImGui)
├── pserv5.cpp                # Entry point (WinMain)
└── precomp.h/.cpp            # PCH
```

## Technology Stack

- **C++20** with MSVC
- **ImGui** (DirectX 11 backend)
- **spdlog** (rotating file logging)
- **WIL** (Windows Implementation Library for RAII handles)
- **toml++** (configuration persistence)
- **MSBuild** (Visual Studio 2022 projects)

All dependencies as git submodules: `imgui/`, `spdlog/`, `wil/`, `tomlplusplus/`

## Build Notes

**IMPORTANT**: Do NOT attempt automated builds via command line. MSBuild CLI has issues. After code changes, inform user to build manually in Visual Studio.

## Configuration

Settings persist to: `%APPDATA%\pserv5\config.toml`

Structure:
```toml
[Application]
AutoRefreshEnabled = true
AutoRefreshInterval = 5
Theme = "dark"

[Window]
Width = 1280
Height = 720
Maximized = false

[Services]
[[Services.Columns]]
Name = "DisplayName"
Width = 200
Visible = true
```

Access pattern:
```cpp
#include "config/settings.h"

bool autoRefresh = theSettings.application.autoRefreshEnabled.get();
theSettings.application.autoRefreshInterval.set(10);
theSettings.save();  // Persist to disk
```

## Windows API Patterns

All Windows API wrappers return `std::vector<T*>` where ownership transfers to caller:
```cpp
// Wrapper returns owned pointers:
std::vector<ServiceInfo*> ServiceManager::EnumerateServices() {
    std::vector<ServiceInfo*> result;
    // ... enumerate and create objects ...
    return result;  // Caller must delete
}

// Caller takes ownership:
void Controller::Refresh() {
    Clear();  // Delete old
    m_services = ServiceManager::EnumerateServices();  // Take ownership
}
```

Error handling:
```cpp
// Wrappers throw std::runtime_error with GetLastError() message
try {
    ServiceManager::StartServiceByName(name);
} catch (const std::exception& e) {
    spdlog::error("Failed: {}", e.what());
    MessageBoxA(hwnd, e.what(), "Error", MB_OK | MB_ICONERROR);
}
```

WIL handles auto-cleanup:
```cpp
wil::unique_schandle hSCM(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
if (!hSCM) throw std::runtime_error(GetLastWin32ErrorMessage());
// Automatic CloseServiceHandle() on scope exit
```

## Current Work: Column Type System Refactoring

**Problem Statement:**
- All columns are left-aligned (numbers should be right-aligned)
- Memory sizes stored/sorted as strings ("57,44 MB") instead of numerically
- Code duplication: Uninstaller has FormatSize() + numeric sorting, Processes doesn't
- Column metadata is minimal (just display name + property ID)
- Generic string sorting everywhere, hardcoded index checks for numeric columns

**Goal:**
Columns should have type information (String vs Numeric) and display format (Raw vs HumanReadableSize) to enable proper alignment, sorting, and reduce code duplication.

## Future Work

The core application is feature-complete. Remaining tasks for future releases:

**Command-Line Interface**
- Headless mode support
- XML export/import for all views
- Service automation commands
- Reference: pserv4 XML format for compatibility

**Polish & Beta Release**
- Memory leak detection (Application Verifier)
- Performance profiling (identify hotspots)
- WiX MSI installer project
- Code signing for enterprise distribution

All completed refinement tasks (TASK-001 through TASK-010) are documented in Git history.

## EXPORT-001 through EXPORT-006: Common Export/Copy Actions

**Problem Statement:**
Users need to export and copy data in multiple formats (JSON, plaintext) across all views. Currently, each controller implements ad-hoc copying (e.g., ModulesDataController::CopyInfo with hardcoded formatting). This leads to:
- Code duplication across controllers
- Inconsistent output formats
- No unified export-to-file functionality
- No JSON export capability

**Goal:**
Create a generic export system with pluggable formatters that works for all data types. Controllers can add standardized "Export to JSON", "Copy as JSON", "Export to TXT", "Copy as TXT" context menu entries with zero per-controller implementation code.

**Design:**
```cpp
// Core abstraction
class IExporter {
    virtual std::string ExportSingle(const DataObject*, const std::vector<DataObjectColumn>&) = 0;
    virtual std::string ExportMultiple(const std::vector<const DataObject*>&, const std::vector<DataObjectColumn>&) = 0;
    virtual std::string GetFormatName() const = 0;  // "JSON", "Plain Text", etc.
    virtual std::string GetFileExtension() const = 0;  // ".json", ".txt"
};

// Implementations
class JsonExporter : public IExporter;       // Uses RapidJSON
class PlainTextExporter : public IExporter;  // Human-readable key: value format

// Action enum in data_controller.h
enum class CommonAction {
    Separator = -1,
    ExportToJson = -1000,
    CopyAsJson = -1001,
    ExportToTxt = -1002,
    CopyAsTxt = -1003
};

// Helper in DataController base class
void AddCommonExportActions(std::vector<int>& actions) const;
void DispatchCommonAction(int action, DataActionDispatchContext& context);
```

**Milestones:**

### EXPORT-001: Create IExporter interface and base infrastructure
- Create `core/exporters/exporter_interface.h` with IExporter abstract class
- Add `core/exporters/exporter_registry.h/.cpp` to manage available exporters
- Add CommonAction enum to `core/data_controller.h`
- Add helper methods to DataController base class:
  * `AddCommonExportActions(std::vector<int>& actions)` - appends common actions
  * `DispatchCommonAction(int action, DataActionDispatchContext& context)` - handles dispatch
- Create `utils/file_dialogs.h/.cpp` with SaveFileDialog() wrapper using IFileSaveDialog COM

**Success criteria:**
- Interface compiles
- ExporterRegistry singleton can register/retrieve exporters
- SaveFileDialog() can prompt for file path with filter

### EXPORT-002: Implement JsonExporter
- Create `core/exporters/json_exporter.h/.cpp`
- Implement ExportSingle():
  * Create JSON object with property name -> value mapping
  * Use column metadata from GetColumns() to enumerate properties
  * Call GetProperty(columnIndex) for each column
  * Pretty-print with 2-space indentation
- Implement ExportMultiple():
  * Create JSON array of objects
  * Use ExportSingle() for each object
- Register in ExporterRegistry during static initialization

**Success criteria:**
- Single object exports to valid, pretty-printed JSON
- Multiple objects export to JSON array
- All columns from controller metadata included
- Empty/null values handled gracefully

### EXPORT-003: Implement PlainTextExporter
- Create `core/exporters/plaintext_exporter.h/.cpp`
- Implement ExportSingle():
  * Format as "PropertyName: Value\n" for each column
  * Use column display names from metadata
  * Add blank line between objects
- Implement ExportMultiple():
  * Call ExportSingle() for each object with separator
  * Add object count header ("Exported 5 services:")
- Register in ExporterRegistry

**Success criteria:**
- Single object exports to human-readable text
- Multiple objects clearly separated
- Format matches existing ad-hoc implementations (e.g., ModulesDataController::CopyInfo)

### EXPORT-004: Integrate common actions into DataController base
- Implement DataController::AddCommonExportActions():
  * Query ExporterRegistry for available exporters
  * Add separator if actions list not empty
  * For each exporter, add Copy/Export action pairs
  * Use negative action IDs (< -1000) to avoid collision with controller-specific actions
- Implement DataController::DispatchCommonAction():
  * Route to appropriate exporter based on action ID
  * For Copy actions: call exporter, put result in clipboard via ImGui::SetClipboardText()
  * For Export actions: show SaveFileDialog, write result to file
  * Handle errors with MessageBox and spdlog

**Success criteria:**
- Base class can dispatch common actions without derived class involvement
- Clipboard operations work
- File save with proper extension filtering works
- Errors logged and shown to user

### EXPORT-005: Update all controllers to use common actions
- Modify GetAvailableActions() in all 5 controllers:
  * ServicesDataController
  * ProcessesDataController
  * WindowsDataController
  * ModulesDataController (remove existing CopyInfo action)
  * UninstallerDataController
- Call AddCommonExportActions() at end of action list
- Modify DispatchAction() to call DispatchCommonAction() for unhandled actions:
  ```cpp
  default:
      // Delegate to base class for common actions
      DispatchCommonAction(action, context);
      break;
  ```

**Success criteria:**
- All 5 controllers show Export/Copy options for JSON and TXT
- ModulesDataController's ad-hoc CopyInfo removed, replaced by generic version
- No code duplication across controllers

### EXPORT-006: Test and document
- Test each controller's export functionality:
  * Single object export/copy
  * Multiple object export/copy
  * Empty selection handling
  * Large datasets (100+ objects)
  * Special characters in data (quotes, newlines, Unicode)
- Verify JSON validity with online validator
- Update instructions.md with Export System section
- Add usage example to architecture documentation

**Success criteria:**
- All views can export/copy in both formats
- JSON is valid and parseable
- Text format is readable and consistent
- Documentation complete

**Implementation Notes:**
- RapidJSON already included at `..\rapidjson\include`
- Use RapidJSON's StringBuffer + PrettyWriter for output
- Export should respect column order from controller metadata
- File dialog should remember last used directory (persist in config?)
- Consider adding XML exporter in future (for pserv4 compatibility)
- Consider adding CSV exporter for spreadsheet import

## Historical Context

- **1998 (v1)**: Custom Windows library
- **2002 (v2)**: MFC rewrite
- **2010 (v3)**: C# Windows Forms
- **2014 (v4)**: C# WPF with MahApps.Metro
- **2025 (v5)**: C++20 with ImGui (this version)

pserv5 maintains feature parity with pserv4 while eliminating .NET runtime dependency and improving performance.

## UNsorted