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

**Current completion: ~90%**

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

## Future Work

### New Data Sources (Next Priority)

The following data sources fit the existing DataController/DataObject/DataAction paradigm and would add significant value:

**1. Scheduled Tasks**
- Enumerate via Task Scheduler API (`ITaskService`, `ITaskFolder`)
- Columns: Name, Status, Trigger, Last Run, Next Run, Author, Enabled
- Actions: Run Now, Enable/Disable, Delete, Edit Configuration
- State: Enabled/Disabled/Running
- Similar complexity to Services view

**2. Startup Programs**
- Enumerate from multiple sources:
  - Registry: `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run`
  - Registry: `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Run`
  - Startup folders: `%ProgramData%\Microsoft\Windows\Start Menu\Programs\Startup`
  - Startup folders: `%AppData%\Microsoft\Windows\Start Menu\Programs\Startup`
  - Task Scheduler startup tasks
- Columns: Name, Location, Type (Registry/Folder/Task), Command, Enabled
- Actions: Enable/Disable, Open Location, Delete, Open in Registry
- Useful for system optimization and malware detection

**3. Network Connections**
- Enumerate via `GetExtendedTcpTable()` and `GetExtendedUdpTable()`
- Columns: Protocol, Local Address, Local Port, Remote Address, Remote Port, State, PID, Process Name
- Actions: Close Connection, Copy Address, Filter by Process
- State: Established/Listening/Close_Wait/Time_Wait/etc.
- Useful for security analysis and troubleshooting

**4. Environment Variables**
- Enumerate from system and user registry locations:
  - System: `HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment`
  - User: `HKCU\Environment`
- Columns: Name, Value, Scope (System/User)
- Actions: Add, Edit, Delete, Copy Value
- Fully editable with transaction-based updates
- Developer-focused utility

### Implementation Notes
- All four sources follow the existing pattern: Manager class in `windows_api/`, Model in `models/`, Controller in `controllers/`, Actions in `actions/`
- Scheduled Tasks: Requires COM (similar to file dialogs), use WIL for handle management
- Startup Programs: Mix of registry enumeration and filesystem scanning
- Network Connections: Uses IP Helper API, needs inet_ntoa() for address formatting
- Environment Variables: Pure registry operations, simplest of the four

### Command-Line Interface
- Headless mode support
- XML export/import for all views
- Service/process automation commands
- Reference: pserv4 XML format for compatibility

### Polish & Beta Release
- Memory leak detection (Application Verifier)
- Performance profiling
- WiX MSI installer project
- Code signing for enterprise distribution

## Historical Context

- **1998 (v1)**: Custom Windows library
- **2002 (v2)**: MFC rewrite
- **2010 (v3)**: C# Windows Forms
- **2014 (v4)**: C# WPF with MahApps.Metro
- **2025 (v5)**: C++20 with ImGui (this version)

pserv5 maintains feature parity with pserv4 while eliminating .NET runtime dependency and improving performance.
