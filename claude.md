# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@agents.md

## Project Overview

**pserv5** is a Windows system administration tool (C++20, ImGui, DirectX 11) for managing services, devices, processes, windows, modules, installed programs, environment variables, startup programs, network connections, and scheduled tasks. Originally from 1998, this is the 5th iteration.

## Build Instructions

**DO NOT attempt automated builds**. MSBuild has CLI issues. After code changes, inform the user that changes are ready for testing - they will build manually in Visual Studio 2022.

Solution: `pserv5.slnx`
Output: `pserv5/bin/x64/Release/pserv5.exe` (GUI), `pservc.exe` (CLI)

## Architecture

### Three-Part Pattern

Every feature follows Model/Controller/Action separation:

1. **DataObject** (`models/`) - Single data item (e.g., `ServiceInfo`, `ProcessInfo`)
   - `GetProperty(int)` - Display string for column
   - `GetTypedProperty(int)` - Native type for sorting
   - `MatchesFilter(string)` - Filter matching

2. **DataController** (`controllers/`) - Business logic, UI-independent
   - `Refresh()` - Load data from Windows APIs
   - `GetColumns()` - Column definitions
   - `GetActions()` - Available actions for selection
   - `GetVisualState()` - Row rendering state

3. **DataAction** (`actions/`) - Self-contained command objects
   - `IsAvailableFor(obj)` - Dynamic availability
   - `Execute(context)` - Perform operation
   - `IsDestructive()` / `RequiresConfirmation()` - UI hints

### Key Directories

```
pserv5/
├── core/           # Base classes: DataObject, DataController, DataAction
├── models/         # Data objects (ServiceInfo, ProcessInfo, etc.)
├── controllers/    # Business logic (ServicesDataController, etc.)
├── actions/        # Action implementations
├── windows_api/    # Windows API wrappers (ServiceManager, etc.)
├── config/         # TOML-based configuration system
├── utils/          # Logging, string conversion, error handling
├── dialogs/        # UI dialogs
└── pservc/         # Console application (shares core with GUI)
```

## Coding Conventions

- **Naming**: MFC style - `m_name` for members, `m_pPointer` for pointers, `m_bBool` for booleans
- **Strings**: UTF-8 internally, convert to UTF-16 only at Windows API boundaries
  ```cpp
  std::wstring wide = Utf8ToWide(utf8String);
  std::string utf8 = WideToUtf8(wideString);
  ```
- **Error handling**: Use macros from `utils/win32_error.h`
  ```cpp
  LogWin32Error("FunctionName", "context: {}", value);      // Uses GetLastError()
  LogWin32ErrorCode("FunctionName", status, "context");     // Direct error code
  ```
- **Memory**: Controllers own DataObjects; UI borrows pointers. DataObject has ref-counting.

## Adding a New View

1. Create model in `models/` (derive from `DataObject`)
2. Create Windows API wrapper in `windows_api/`
3. Create controller in `controllers/` (derive from `DataController`)
4. Create actions in `actions/` (derive from `DataAction`)
5. Register in `core/data_controller_library.cpp`
6. Add config section in `config/settings.h`

Console support is automatic - controllers register themselves in pservc.

## Dependencies (Git Submodules)

- ImGui - GUI framework
- spdlog - Logging
- WIL - Windows Implementation Library (RAII handles, COM pointers)
- toml++ - Configuration
- RapidJSON - JSON export
- argparse - CLI parsing

## Key Files

- `core/data_object.h` - Base class for all data items
- `core/data_controller.h` - Base controller class
- `core/data_action.h` - Base action class
- `main_window.h/.cpp` - Main UI window
- `config/settings.h` - Global configuration
- `utils/win32_error.h` - Error handling macros
- `utils/string_utils.h` - UTF-8/UTF-16 conversion
