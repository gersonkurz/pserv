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

### Implementation Plan

**COLUMN-001: Extend Column Metadata**
- Add `ColumnDataType` enum to `core/data_object_column.h` (String, Integer, UnsignedInteger, Size, Time)
- Add `ColumnAlignment` enum (Left, Right)
- Extend `DataObjectColumn` class with type and alignment fields
- Update all controller constructors to specify types for each column
- Non-breaking: Pure metadata addition, no behavior change

**COLUMN-002: Centralize Size Formatting**
- Create `utils/format_utils.h` and `utils/format_utils.cpp`
- Move `FormatSize()` from `windows_api/uninstaller_manager.cpp` to new utility
- Make it shared: `std::string FormatSize(uint64_t bytes)`
- Update UninstallerManager to use centralized version
- Non-breaking: Code consolidation only

**COLUMN-003: Refactor ProcessInfo for Raw Values**
- Change ProcessInfo to store raw SIZE_T values for memory fields
- Add typed getters: `GetWorkingSetSizeBytes()`, `GetPrivatePageCountBytes()`, etc.
- Keep `GetProperty(int)` for display (formats using FormatSize())
- Update ProcessManager to populate raw values instead of pre-formatting
- Breaking: Changes ProcessInfo internal storage

**COLUMN-004: Smart Sorting in Controllers**
- Update controller `Sort()` methods to check column data type from metadata
- For Size/Integer columns: get raw numeric value, compare numerically
- For String columns: compare as strings (existing behavior)
- Remove hardcoded column index checks (e.g., `if (columnIndex == 4)`)
- Use metadata-driven approach: `if (columns[columnIndex].GetDataType() == ColumnDataType::Size)`
- Breaking: Changes sort behavior (but fixes bugs)

**COLUMN-005: UI Alignment from Metadata**
- Update `main_window.cpp` table rendering
- Read column alignment from `DataObjectColumn` metadata
- Apply to `ImGui::TableSetupColumn()` with `ImGuiTableColumnFlags_WidthFixed` + alignment
- Right-align numeric columns, left-align string columns
- Breaking: Changes visual appearance

**Status:** Not started. Will implement phases sequentially with verification after each.

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

## Historical Context

- **1998 (v1)**: Custom Windows library
- **2002 (v2)**: MFC rewrite
- **2010 (v3)**: C# Windows Forms
- **2014 (v4)**: C# WPF with MahApps.Metro
- **2025 (v5)**: C++20 with ImGui (this version)

pserv5 maintains feature parity with pserv4 while eliminating .NET runtime dependency and improving performance.
