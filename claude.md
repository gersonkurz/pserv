# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@agents.md

## Project Overview

**pserv5** is a Windows system administration tool (C++20, ImGui, DirectX 11) for managing services, devices, processes, windows, modules, installed programs, environment variables, startup programs, network connections, and scheduled tasks. Originally from 1998, this is the 5th iteration.

## Build Instructions

Build with `just` (recipes in the repo-root `justfile`) from a **Visual Studio 2026** Developer shell (the `_msbuild` recipe guards on `VisualStudioVersion=18.0`):

- `just build` — Debug, native arch (builds every project in the solution)
- `just test` — `just build`, then run `pserv_core_tests` (exit code 0 = all checks pass)
- `just build-release` — Release, native arch
- `just build-all` — Debug+Release across x64, x86, ARM64
- `just run` / `just run-cli [-- args]` — build Debug + launch GUI / CLI
- `just clean`, `just rebuild`
- `just release` — build x64+x86+ARM64 Release, then the 3 MSIs + multi-arch bundle `.exe`
- `just version X.Y.Z` — stamp `pserv5/version.h` + `setup/*.msis` (the `.rc` reads `version.h`)

Builds are solution-driven (`pserv5/pserv5.slnx`) so `$(SolutionDir)` resolves — building a bare `.vcxproj` leaves output paths undefined. The `CopyThemes` MSBuild target copies `pserv5/themes/*.yaml` into the output dir next to the exe.

Solution: `pserv5/pserv5.slnx` (projects: `pserv5.vcxproj` GUI, `pservc/pservc.vcxproj` CLI, `pserv_core/pserv_core.vcxproj` shared core static library linked by the CLI and the tests, `tests/pserv_core_tests.vcxproj`). The GUI still compiles the core sources itself, in its own flavour, until Phase 1 of `docs/pserv6-plan.md`.
Output: `pserv5/bin/<Platform>/<Config>/` — e.g. `pserv5/bin/x64/Release/pserv5.exe` (GUI), `pservc.exe` (CLI). Note the x86 solution platform builds into `bin/Win32/`. `pserv_core_tests.exe` builds into `pserv5/bin/tests/<Platform>/<Config>/`, outside the folder `just` stages into the installers.
Platforms: x64, x86, arm64.

### Packaging (MSI via MSIS 3.x)

Installers are built with [MSIS](https://github.com/gersonkurz/msis) (`msis` on PATH) from `.msis` scripts in `setup/`: per-arch `pserv5-{x64,x86,arm64}.msis` (share one product `UPGRADE_CODE`) and `setup-bundle.msis` (combines all three into one arch-selecting `.exe`). `just release` stages each Release payload — `pserv5.exe` + `pservc.exe` + `themes/` — into `dist/stage/<arch>/`, builds an MSI per arch (standalone, with vcredist launch conditions), then the bundle. Output lands in `setup/` (MSIs + `pserv5-<ver>.exe`); `dist/` and the generated `.msi`/`.exe` are gitignored. (Replaced the former NSIS `.nsi`/`rebuild-*.cmd` scripts.)

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

## Review loop

@C:/Projects/yaaadabi/protocol.md

Loop parameters:
- Verify: just build-all && just test
- Yardstick docs: docs/pserv6-plan.md (the approved plan; its phase exit criteria are binding), AGENTS.md, CLAUDE.md
- Review focus: object lifetime and refcounting across refresh, async work and the UI/worker thread boundary; GUI code leaking into the shared core; installer payload, elevation and upgrade behaviour; claims stronger than the executed evidence
