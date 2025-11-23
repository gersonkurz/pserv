# pserv5 Development Reference

## Project Status

### Completed
- ✅ Foundation: ImGui + DirectX 11, spdlog logging, TOML configuration, tab infrastructure
- ✅ Windows API wrappers: ServiceManager, ProcessManager, WindowManager, ModuleManager, UninstallerManager
- ✅ Error Handling: Standardized Win32/COM error logging with LogWin32Error macros
- ✅ All Views: Services, Devices, Processes, Windows, Modules, Uninstaller - complete with enumeration and management
- ✅ Action System: DataAction abstraction with action objects for all controllers
- ✅ Properties Dialog: Generic DataPropertiesDialog with action buttons and property editing
- ✅ Export System: JSON and plaintext export/copy functionality for all views
- ✅ UI: Custom title bar, modern menu bar, DPI-aware rendering, context menus, async operations

**Current completion: ~95%** (All major views implemented, auto-refresh complete)

## Core Architecture

### Design Principles
1. **UI Independence**: Controllers contain zero UI framework code - enables future UI ports
2. **Separation of Concerns**: Data (models) / Logic (controllers) / Presentation (UI)
3. **Manual Memory Management**: Raw pointers with `new`/`delete` - explicit ownership
4. **UTF-8 Everywhere**: Convert to UTF-16 only at Windows API boundaries
5. **MFC Conventions**: `m_` prefix for members, `m_b` for booleans, `m_p` for pointers

### Three-Part Pattern

**DataObject (Model)**
- Base class for ServiceInfo, ProcessInfo, WindowInfo, etc.
- `GetId()`, `GetProperty(int)`, `GetVisualState()`
- See: `core/data_object.h`, `models/service_info.h`

**DataController (Business Logic)**
- UI-independent controller per view type
- Provides: `Refresh()`, `GetColumns()`, `GetActions()`, `GetVisualState()`
- Owns data objects, manages their lifecycle
- See: `core/data_controller.h`, `controllers/services_data_controller.cpp`

**DataAction (Action Abstraction)**
- Base class for all executable actions (Start, Stop, Terminate, etc.)
- Self-contained: knows its name, availability, and execution logic
- Visibility flags control where actions appear (context menu, properties dialog, both)
- See: `core/data_action.h`, `actions/service_actions.cpp`

### Memory Management

Controllers own their data objects:
```cpp
std::vector<DataObject*> m_objects;

void Refresh() {
    Clear();  // Delete old objects
    m_objects = Manager::Enumerate();  // Ownership transferred
}

void Clear() {
    for (auto* obj : m_objects) delete obj;
    m_objects.clear();
}

~Controller() { Clear(); }
```

UI borrows pointers - no ownership:
```cpp
const auto& objects = controller->GetDataObjects();
for (auto* obj : objects) {
    RenderRow(obj);  // Just renders, doesn't delete
}
```

### Async Operations

Long-running operations use `AsyncOperation`:
```cpp
ctx.m_pAsyncOp = new AsyncOperation();
ctx.m_bShowProgressDialog = true;

ctx.m_pAsyncOp->Start(ctx.m_hWnd, [items](AsyncOperation* op) -> bool {
    for (size_t i = 0; i < items.size(); ++i) {
        op->ReportProgress((float)i / items.size(), "Processing...");
        DoWork(items[i]);
    }
    return true;
});
```

### Error Handling

Standardized Win32/COM error logging:
```cpp
// For APIs using GetLastError()
if (!CreateDirectoryW(path, nullptr)) {
    LogWin32Error("CreateDirectoryW", "path '{}'", pathUtf8);
}

// For APIs returning error codes directly (registry, HRESULT)
LSTATUS status = RegOpenKeyW(...);
if (status != ERROR_SUCCESS) {
    LogWin32ErrorCode("RegOpenKeyW", status, "key '{}'", keyName);
}

// For expected/common failures (debug level logging)
if (!GetUserNameA(buffer, &size)) {
    LogExpectedWin32Error("GetUserNameA");
}
```

See: `utils/win32_error.h`

## Project Structure

```
pserv5/
├── core/                     # Base abstractions
│   ├── data_controller.h/.cpp
│   ├── data_object.h
│   ├── data_object_column.h
│   ├── data_action.h
│   └── async_operation.h/.cpp
│
├── models/                   # Data objects
│   ├── service_info.h/.cpp
│   ├── device_info.h
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
│   ├── modules_data_controller.h/.cpp
│   └── uninstaller_data_controller.h/.cpp
│
├── windows_api/              # Windows API wrappers
│   ├── service_manager.h/.cpp
│   ├── process_manager.h/.cpp
│   ├── window_manager.h/.cpp
│   ├── module_manager.h/.cpp
│   └── uninstaller_manager.h/.cpp
│
├── actions/                  # Action implementations
│   ├── common_actions.h/.cpp
│   ├── service_actions.h/.cpp
│   ├── process_actions.h/.cpp
│   └── ...
│
├── utils/                    # Utilities
│   ├── string_utils.h        # Utf8ToWide/WideToUtf8
│   ├── win32_error.h         # Error logging macros
│   └── file_dialogs.h/.cpp   # SaveFileDialog wrapper
│
├── config/                   # Configuration system
│   ├── settings.h/.cpp       # Global theSettings
│   └── toml_backend.h
│
└── main_window.h/.cpp        # UI layer (ImGui)
```

## Technology Stack

- **C++20** with MSVC
- **ImGui** (DirectX 11 backend)
- **spdlog** (rotating file logging)
- **WIL** (Windows Implementation Library for RAII handles)
- **toml++** (configuration persistence)
- **RapidJSON** (JSON export)
- **MSBuild** (Visual Studio 2022)

All dependencies as git submodules: `imgui/`, `spdlog/`, `wil/`, `tomlplusplus/`, `rapidjson/`

## Build Notes

**IMPORTANT**: Do NOT attempt automated builds via command line. MSBuild CLI has issues. After code changes, inform user to build manually in Visual Studio.

## Configuration

Settings persist to: `%APPDATA%\pserv5\config.toml`

Access pattern:
```cpp
#include "config/settings.h"

bool autoRefresh = theSettings.application.autoRefreshEnabled.get();
theSettings.application.autoRefreshInterval.set(10);
theSettings.save();  // Persist to disk
```

## Pre-Release Gap Analysis

### Critical Issues
1. **Window Handle Missing in Properties Dialog** (`data_properties_dialog.cpp:436`)
   - `ctx.m_hWnd = nullptr` - prevents proper dialog parenting
   - Actions from properties dialog may show orphaned dialogs

2. **Debug Log File Deletion** (`utils/logging.cpp:59`)
   - `std::remove("pserv5.log")` executes on every startup
   - Prevents post-crash diagnostics

### Incomplete Features
1. **Add Environment Variable** (`environment_variable_actions.cpp:158`)
   - Shows "Not Yet Implemented" placeholder
   - Either implement or hide action

2. **Close TCP Connection** (`network_connection_actions.cpp:130`)
   - Shows "Not Yet Implemented" placeholder
   - Requires `SetTcpEntry` implementation or hide action

3. **Missing Auto-Refresh After Deletes** (3 locations)
   - Environment variables, scheduled tasks, startup programs
   - User must manually refresh after delete operations
   - Files: `environment_variable_actions.cpp:130`, `scheduled_task_actions.cpp:178`, `startup_program_actions.cpp:145`

### TODO

2) **Console Variant**
   - Create a console project pservc that is in the same solution as pserv5, but without using ImGui. 
   - If built with _CONSOLE, exclude all ImGui / UI specifics from the build
   - Instead, use ../argparse (a git submodule from https://github.com/jamolnng/argparse.git) and populate it with actions / commands for all controllers and THEIR commands. 
   - Ideally, the full pserv5 UI functionality that is non-GUI related (such as "property dialogs", "font resizing", "column width", "auto-refresh" and so on) is made available on the console as well.

3) **Minor Bugfixing**
   - Prevent generation of imgui.ini if we can help it. If we cannot, then move it to the %APPDATA% folder for us.

4) **Github Presentation**
   - Prepare the github webpage, add readme and license references, and optionally check out the possibility for uploading binaries.
   - Verify: MIT License is good for our use of third-party libraries

5) **Switch to fmt::format**
   - So that we can support color output on the console.

6) **Code cleanup**
   - Systematically document all classes / methods, ideally in a system that can be exported (e.g. doxgen or modern alternatives)
   - Perform a security review via AI
   - Remove 32-bit build from pserv5/pservc
   - Instead, add ARM64 build for Parallels Desktop use on Macbook Pro M4

## Historical Context

- **1998 (v1)**: Custom Windows library
- **2002 (v2)**: MFC rewrite
- **2010 (v3)**: C# Windows Forms
- **2014 (v4)**: C# WPF with MahApps.Metro
- **2025 (v5)**: C++20 with ImGui (this version)

pserv5 maintains feature parity with pserv4 while eliminating .NET runtime dependency and improving performance.
