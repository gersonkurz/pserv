# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@agents.md

## Project Overview

**pserv6** is a Windows system administration tool (C++, WinUI 3) for managing services, devices, processes, windows, modules, installed programs, environment variables, startup programs, network connections, and scheduled tasks, with a console companion `pservc`. Originally from 1998, this is the 6th iteration; pserv5's Dear ImGui frontend was removed in favour of pserv6 (see `docs/pserv6-plan.md`).

## Build Instructions

Build with `just` (recipes in the repo-root `justfile`) from a **Visual Studio 2026** Developer shell (the `_msbuild` recipe guards on `VisualStudioVersion=18.0`):

- `just build` — Debug, native arch (builds every project in the solution)
- `just test` — `just build`, then run `pserv_core_tests` and check that every built `pserv6.exe` embeds an `asInvoker` manifest (exit code 0 = all checks pass)
- `just test-asan` — `pserv_core` + tests under AddressSanitizer (Debug, native arch, `-asan` output folders), with the 50-round background-refresh stress run. Native ARM64 on ARM64 hardware: the x64 ASan runtime does not start under emulation.
- `just build-release` — Release, native arch
- `just build-all` — Debug+Release for x64, x86 and ARM64 (solution platform `x86` builds into `bin\Win32`)
- `just run` / `just run-cli [-- args]` — build Debug + launch pserv6 (it starts with the shell's rights; "Run as administrator" in the window restarts it elevated) / pservc
- `just selftest` — build Release + run `pserv6 --selftest` (every view loaded, scrolled, refreshed; grid checks; timing log vs. the plan's section 7 budgets; exit 1 unless `RESULT: PASS` and exit code 0)
- `just clean`, `just rebuild`
- `just release` — build x64, x86 and ARM64 Release, then the 3 MSIs + the multi-arch bundle `.exe`
- `just check-msi <x64|x86|arm64>` — check a built MSI without installing it (administrative extract compared with the staged payload); `scripts\check-msi.ps1 -SelfTest` also runs the extracted pserv6
- `just version X.Y.Z` — stamp `pserv6/version.h` + `setup/*.msis` (the `.rc` files read `version.h`)

Builds are solution-driven (`pserv6/pserv6.slnx`) so `$(SolutionDir)` resolves — building a bare `.vcxproj` leaves output paths undefined.

Solution: `pserv6/pserv6.slnx` (projects: `gui/pserv6.vcxproj` the WinUI 3 GUI, `pservc/pservc.vcxproj` CLI, `pserv_core/pserv_core.vcxproj` shared core static library linked by the GUI, the CLI and the tests, `tests/pserv_core_tests.vcxproj`). The core is frontend-neutral: one compiled library, no frontend macros (see AGENTS.md).
Output: `pserv6/bin/<Platform>/<Config>/` — `pserv6.exe` with its self-contained WinAppSDK runtime files (unpackaged; the WinUI build recipe and its pitfalls are Appendix A of `docs/pserv6-plan.md`) and `pservc.exe`; this folder is the release payload. `pserv_core_tests.exe` builds into `pserv6/bin/tests/<Platform>/<Config>/`, outside it.
Platforms: x64, x86, ARM64.

Settings: `%APPDATA%\pserv6\config.toml`, shared by pserv6 (reads and saves) and pservc (reads only); on the first run pserv5's `%LOCALAPPDATA%\pserv5\pserv5.toml` is imported. Logs: `pserv6.log` and `pservc.log` in the same folder.

### Packaging (MSI via MSIS 3.x)

Installers are built with [MSIS](https://github.com/gersonkurz/msis) (`msis` on PATH) from `.msis` scripts in `setup/`: per-arch `pserv6-{x64,x86,arm64}.msis` and `setup-bundle.msis` (one arch-selecting `.exe`). They keep **pserv5's UpgradeCodes** (plan D12: one lineage), so installing pserv6 upgrades an installed pserv5 away — test installs belong in a disposable environment, not on a dev box. `just release` stages each Release payload into `dist/stage/<arch>/` (symbols into `dist/symbols/<arch>/`; they are the MSI's optional "Debug Symbols" feature, not installed by default), builds an MSI per arch (standalone, with vcredist launch conditions), then the bundle. Output lands in `setup/` (`pserv6-<ver>-<arch>.msi`, `pserv6-<ver>.exe`); `dist/` and the generated `.msi`/`.exe` are gitignored.

## Architecture

### Three-Part Pattern

Every feature follows Model/Controller/Action separation:

1. **DataObject** (`models/`) - Single data item (e.g., `ServiceInfo`, `ProcessInfo`)
   - `GetProperty(int)` - Display string for column
   - `GetTypedProperty(int)` - Native type for sorting
   - `MatchesFilter(string)` - Filter matching

2. **DataController** (`controllers/`) - Business logic, UI-independent
   - `Collect(target, request)` - Enumerate from Windows APIs (on the refresh worker)
   - `GetColumns()` - Column definitions
   - `GetActions()` - Available actions for selection
   - `GetVisualState()` - Row rendering state

3. **DataAction** (`actions/`) - Self-contained command objects
   - `IsAvailableFor(obj)` - Dynamic availability
   - `Execute(context)` - Perform operation
   - `IsDestructive()` / `RequiresConfirmation()` - UI hints

### Key Directories

```
pserv6/
├── gui/            # pserv6.exe: WinUI 3 window, grid, self-test
├── pservc/         # pservc.exe: console application
├── core/           # Base classes: DataObject, DataController, DataAction, refresh coordinator
├── models/         # Data objects (ServiceInfo, ProcessInfo, etc.)
├── controllers/    # Business logic (ServicesDataController, etc.)
├── actions/        # Action implementations
├── windows_api/    # Windows API wrappers (ServiceManager, etc.)
├── config/         # TOML-based configuration system
├── utils/          # Logging, string conversion, error handling, app bootstrap
├── pserv_core/     # The core's static-library project
└── tests/          # pserv_core_tests
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

1. Create model in `models/` (derive from `CloneableDataObject`)
2. Create Windows API wrapper in `windows_api/`
3. Create controller in `controllers/` (derive from `DataController`)
4. Create actions in `actions/` (derive from `DataAction`)
5. Register in `core/data_controller_library.cpp`
6. Add config section in `config/settings.h`

Both frontends pick up registered controllers automatically.

## Dependencies (Git Submodules)

- spdlog - Logging
- WIL - Windows Implementation Library (RAII handles, COM pointers)
- toml++ - Configuration
- RapidJSON - JSON export
- argparse - CLI parsing

The GUI also uses NuGet packages (Windows App SDK WinUI + Foundation, C++/WinRT), restored by `/restore`.

## Key Files

- `core/data_object.h` - Base class for all data items
- `core/data_controller.h` - Base controller class
- `core/data_action.h` - Base action class
- `core/refresh_coordinator.h` - Background refresh protocol
- `gui/MainWindow.xaml.h/.cpp` - Main window
- `gui/data_grid.h/.cpp` - The grid
- `config/settings.h` - Global configuration
- `utils/win32_error.h` - Error handling macros
- `utils/string_utils.h` - UTF-8/UTF-16 conversion
