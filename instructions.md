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

## Refinement Tasks

### Bug Fixes (High Priority)

**TASK-001: Fix Uninstaller Refresh Timing** ✅ COMPLETED
- File: `controllers/uninstaller_data_controller.cpp:152-156`
- Issue: Called `Refresh()` immediately after launching uninstaller, list became stale
- Fix: Added `m_bNeedsRefresh` flag to base `DataController` class
- Fix: Controller sets flag instead of calling Refresh() directly
- Fix: UI layer (`main_window.cpp:590-610`) highlights Refresh button in orange when flag is set
- Pattern: Client controls when refresh happens, controller just signals the need

**TASK-002: Fix Uninstaller String Parsing** ✅ COMPLETED
- File: `controllers/uninstaller_data_controller.cpp:122-154`
- Issue: Naive parsing didn't handle quoted paths with spaces correctly
- Example: `"C:\Program Files\Foo\uninstall.exe" /S` broke on first space inside quotes
- Fix: Use Windows `CommandLineToArgvW` API for proper quote handling
- Fix: WIL's `unique_hlocal_ptr` handles memory cleanup automatically
- Fix: Rebuild arguments with proper quoting for ShellExecuteW

**TASK-003: Fix Uninstaller Size Sorting** ✅ COMPLETED
- Files: `models/installed_program_info.{h,cpp}`, `windows_api/uninstaller_manager.{h,cpp}`, `controllers/uninstaller_data_controller.cpp:196-202`
- Issue: Estimated Size column sorted lexicographically ("100 KB" < "90 KB")
- Fix: Added `m_estimatedSizeBytes` uint64_t field to model for numeric storage
- Fix: Read EstimatedSize from registry as DWORD (KB), convert to bytes
- Fix: Added `FormatSize()` helper for human-readable display (e.g., "4.50 GB")
- Fix: Sort uses numeric comparison on bytes, not formatted string
- Result: Proper numeric sorting (90 MB > 10 GB is false, as expected)

**TASK-004: Remove Implicit Refresh in GetDataObjects()** ✅ COMPLETED
- Files: `controllers/processes_data_controller.h:22-29`, `controllers/services_data_controller.h:53-60`
- Issue: Broke const correctness by calling `Refresh()` inside const getter
- Fix: Removed all implicit refresh logic from `GetDataObjects()` implementations
- Note: `main_window.cpp:578-586` already handles lazy refresh correctly via `IsLoaded()` check

**TASK-005: Fix Processes Username Caching** ✅ COMPLETED
- File: `controllers/processes_data_controller.cpp:44-50`
- Issue: Cached username once, never invalidated if user switches or app runs as service
- Fix: Re-query username on every refresh (negligible performance impact vs process enumeration)
- Now handles user switch scenarios and service context changes

### Architectural Issues (Medium Priority)

**TASK-006: Redesign Modules Controller**
- File: `controllers/modules_data_controller.cpp:29-46`
- Issue: Enumerates ALL modules from ALL processes (thousands of objects, very slow)
- Current: Global enumeration like processes/services view
- Expected: Lazy-load modules for *selected* process only
- Pattern: May need parent-child relationship like Windows controller
- Decision needed: Should modules be a separate view, or sub-view of processes?

**TASK-007: Uninstaller Properties Dialog Const-Correctness**
- File: `controllers/uninstaller_data_controller.cpp:107-112`
- Issue: Casts away const to pass to dialog
- Root cause: Dialog expects non-const pointers for editing
- Fix: Make dialog accept const pointers (it's read-only) or document mutability contract

**TASK-008: Services Controller Column Order Comments**
- File: `controllers/services_data_controller.cpp:16-36`
- Issue: Comment says "order matches ServiceProperty enum" but relies on positional magic
- Risk: Adding column breaks numeric sort logic (line 128)
- Fix: Use explicit enum values in sort switch, not column index assumptions

### Code Quality (Low Priority)

**TASK-009: Processes Controller Incomplete Sort**
- File: `controllers/processes_data_controller.cpp:276`
- Issue: PageFaultCount case commented out with "Need getter"
- Fix: Either add getter to ProcessInfo or remove column

**TASK-010: Modules Controller Verbose Comparison**
- File: `controllers/modules_data_controller.cpp:159-160`
- Issue: Ternary chain for pointer comparison could be simplified
- Current: `(a < b) ? -1 : (a > b)`
- Better: `(a < b) - (a > b)` or `std::compare_three_way`

**TASK-011: Inconsistent Sort Patterns**
- Files: All `*_data_controller.cpp` Sort() methods
- Issue: Mix of approaches - some parse strings, some access typed getters
- Fix: Standardize on one pattern (prefer typed getters for numeric columns)

### Future Work

**TASK-012: Command-Line Interface**
- Headless mode support
- XML export/import for all views
- Service automation commands
- Reference: pserv4 XML format for compatibility

**TASK-013: Polish & Beta Release**
- Memory leak detection (Application Verifier)
- Performance profiling (identify hotspots)
- WiX MSI installer project
- Code signing for enterprise distribution

## Historical Context

- **1998 (v1)**: Custom Windows library
- **2002 (v2)**: MFC rewrite
- **2010 (v3)**: C# Windows Forms
- **2014 (v4)**: C# WPF with MahApps.Metro
- **2025 (v5)**: C++20 with ImGui (this version)

pserv5 maintains feature parity with pserv4 while eliminating .NET runtime dependency and improving performance.
