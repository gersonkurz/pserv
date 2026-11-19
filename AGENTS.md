# Conversation Guidelines
Primary Objective: Engage in honest, insight-driven dialogue that advances understanding. I'm European. You don't need to pamper me. Also, I've been programming for 35 years, so don't bullshit me. Remain critical and skeptical about my thinking at all times. Maintain consistent intellectual standards throughout our conversation. Don't lower your bar for evidence or reasoning quality just because we've been talking longer or because I seem frustrated. If I'm making weak arguments, keep pointing that out even if I've made good ones before.

## Core Principles
- Intellectual honesty: Share genuine insights without unnecessary flattery or dismissiveness
- Critical engagement: Push on important considerations rather than accepting ideas at face value
- Balanced evaluation: Present both positive and negative opinions only when well-reasoned and warranted
- Directional clarity: Focus on whether ideas move us forward or lead us astray

## What to Avoid
- Sycophantic responses or unwarranted positivity ("You're absolutely right", "Perfect!", "Great catch!", etc.)
- Dismissing ideas without proper consideration
- Superficial agreement or disagreement
- Flattery that doesn't serve the conversation
- Filler phrases and preambles - just give the meat (the "burger theory" of communication)

## Success Metric
The only currency that matters: Does this advance or halt productive thinking? If we're heading down an unproductive path, point it out directly.

## Project Overview

pserv6 is a native Windows administration tool with a C++/WinRT / WinUI 3 GUI (`ui/`, Windows App SDK 2.7.0) and a console frontend (`cli/`, `pserv6c.exe`). They share controllers, models, actions and Windows API wrappers for ten providers. The obsolete immediate-mode frontend has been removed.

The code uses modern C++; both projects currently select MSVC `stdcpplatest` and toolset `v145`. Check the project files rather than relying on older C++20 or Visual Studio 2022 descriptions.

## Build and Validation

Run recipes from the repository root with `just` in a **Visual Studio 2026 Developer shell**. The build requires `VisualStudioVersion=18.0` and `msbuild` on PATH. Do not merely set that variable to bypass the guard; initialize the VS toolchain environment.

- `just build` — pserv6 Debug, native architecture.
- `just build-release` — pserv6 Release, native architecture.
- `just build-all` — Debug and Release for x64, x86, and ARM64.
- `just run` — build Release and launch pserv6.
- `just run-cli services` — build Release and list services through the CLI.
- `just --list` — current public recipes.

Recipes build `pserv6/pserv6.slnx`, containing `ui/pserv6.vcxproj`, its native static library `core/pserv6-core.vcxproj`, and `cli/pserv6c.vcxproj`. The static library compiles the GUI's shared sources with `precomp.h`; WinUI sources use `ui/pch.h`. Keep separate projects because the XAML generators assume one PCH header per project. The CLI keeps its console-specific compilation. Build the solution: projects depend on `$(SolutionDir)`. Output is `pserv6/bin/<Platform>/<Configuration>/`; x86 uses `Win32`. Intermediates live under `pserv6/temp/`. `scripts/native-platform.ps1` queries IsWow64Process2, selecting ARM64 on ARM64 hosts even under x64 emulation. Do not use PROCESSOR_ARCHITECTURE or .NET Framework OSArchitecture for this detection.

Verify changes compile before handing off. `just test` runs service-edit, environment-edit, row-reconciliation, actual WinRT cell-collection, keyboard/refresh-routing, static-library exporter and multi-object property-edit checks; it requires a Developer shell targeting the native OS architecture. These tests do not create an application window or change system services/variables. For behavior changes, exercise the affected GUI or CLI path in addition to building when the user permits runtime checks, and report what was actually checked. CLI help and listing commands provide read-only smoke checks, for example `pserv6/bin/x64/Debug/pserv6c.exe --help` (adjust for the built architecture). Build success alone does not verify UI behavior.

`just clean` removes all of `pserv6/bin/`, `pserv6/temp/`, and `dist/`; `just rebuild` performs that cleanup before building Debug. Prefer incremental builds unless cleanup is needed.

## Repository Map

Paths below are relative to `pserv6/` unless stated otherwise.

| Location | Responsibility |
| --- | --- |
| `ui/` | WinUI startup, XAML, main window, table bindings and interaction |
| `cli/` | CLI entry point, argument dispatch, console output |
| `core/` | Shared object, container, controller and action abstractions; async operations |
| `core/exporters/` | Export interfaces, registry, JSON and plain-text exporters |
| `models/` | Data items and their display/typed properties |
| `controllers/` | Enumeration, columns, filtering, sorting, available actions |
| `actions/` | Commands operating on selected objects |
| `windows_api/` | Windows resource enumeration and manipulation |
| `config/` | TOML-backed settings and per-view configuration |
| `theme/`, `themes/` | Base16 theme loader and YAML theme files |
| `utils/` | Logging, string conversions, Win32 errors, common app setup |
| Repository `docs/pserv6.md`, `docs/pserv6c.md` | User-facing GUI and CLI manuals |
| Repository `setup/`, `scripts/` | MSIS installers and version-stamping script |

## Architecture and Change Conventions

- Keep shared behavior in the model/controller/action and Windows API layers so both frontends benefit. GUI-only code is excluded from the CLI with `PSERV_CONSOLE_BUILD`; keep XAML types out of shared code.
- `DataObject` supplies display properties, typed properties for sorting, filtering, and stable IDs. Keep column metadata and both property representations consistent when adding or changing columns.
- `DataController` defines columns and actions and refreshes its objects. `DataAction` executes through `DataActionDispatchContext`; preserve availability, destructive-action, and confirmation semantics in both frontends.
- Objects use intrusive `Retain`/`Release` reference counting. `DataObjectContainer::Append` takes the incoming reference; it does not retain it again. `GetByStableId` returns a borrowed pointer and marks the object seen in the current refresh generation. Preserve the `StartRefresh`/`FinishRefresh` lifecycle and check ownership when selections or async work outlive a refresh.
- Use `core/async_operation.h` for existing background-operation patterns, including progress, cancellation, and completion notification. Keep object and dispatch-context lifetimes valid until work completes.
- Follow adjacent code: four-space indentation, braces on separate lines, `pserv` namespaces, and existing `m_` / `m_p` / `m_b` member conventions. Translation units generally include `precomp.h` first.
- Use UTF-8 internally and the helpers in `utils/string_utils.h` for UTF-16 conversion at Windows API boundaries. Use existing WIL RAII types for Windows resources where appropriate.
- Use spdlog and `utils/win32_error.h` for diagnostics. Distinguish APIs reporting `GetLastError()` from APIs returning an error code directly; preserve the relevant error before other calls can overwrite it.

When adding a view:

1. Add the model, controller, Windows API wrapper, and actions as needed, following a comparable existing view.
2. Register the controller in `core/data_controller_library.cpp`; add its name constant in `core/data_controller.h`.
3. Add the per-view settings in `config/settings.h` and the lookup in `config/settings.cpp`.
4. Add source/header entries to the relevant `.vcxproj` files and maintain their `.filters` files. Shared sources must be included in the native core and CLI projects; they are not discovered automatically.
5. Check CLI argument/action registration and behavior. The CLI enumerates the shared controller library, but frontend-specific action handling may still be required.
6. Update the relevant user manual when behavior or commands change.

## Dependencies and Themes

Dependencies are Git submodules: `spdlog`, `wil`, `tomlplusplus`, `rapidjson`, `argparse`, and `fkYAML`. Initialize or resynchronize them with `git submodule update --init --recursive` when needed, particularly after switching revisions. Avoid unrelated changes to vendored code or submodule revisions.

Configuration uses TOML; Base16 themes use YAML parsed with fkYAML. Edit theme sources in `pserv6/themes/`. The GUI project's `CopyThemes` target copies them to `themes/` beside the built executable; output copies are not source files.

## Versioning and Packaging

- `just version X.Y.Z` updates `pserv6/version.h` and `setup/*.msis`; resources consume the version header.
- Packaging requires MSIS 3.x (`msis` on PATH) in addition to the VS build tools.
- `just package-x64`, `just package-x86`, and `just package-arm64` build and stage the respective Release payload and symbols, then create an MSI. `just package-all` builds all three and the final bundle.
- `just bundle` combines the existing per-architecture MSIs; `just release` builds all packages and the bundle.
- Staging uses `dist/stage/<arch>/` and `dist/symbols/<arch>/`. Final MSI and bundle outputs land in `setup/`. Keep generated binaries and staging output out of commits.

Treat the current `justfile`, solution, project files, and implementation as authoritative when older documentation disagrees.
