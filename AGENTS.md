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

pserv5 is a native Windows system administration tool. It has a Dear ImGui / DirectX 11 GUI (`pserv5.exe`) and a console application (`pservc.exe`) that compile shared controllers, models, actions, and Windows API wrappers. Views cover services, devices, processes, windows, modules, installed programs, startup programs, scheduled tasks, network connections, and environment variables.

The code uses modern C++; all projects currently select MSVC `stdcpplatest` and toolset `v145`. Check the project files rather than relying on older C++20 or Visual Studio 2022 descriptions.

## Build and Validation

Run recipes from the repository root with `just` in a **Visual Studio 2026 Developer shell**. The build requires `VisualStudioVersion=18.0` and `msbuild` on PATH. Do not merely set that variable to bypass the guard; initialize the VS toolchain environment.

- `just build` — Debug, native architecture; builds every project in the solution.
- `just test` — `just build`, then run `pserv_core_tests` and check every built `pserv6.exe` manifest for `requireAdministrator`.
- `just test-asan` — the same tests under AddressSanitizer, with the 50-round background-refresh stress run.
- `just build-release` — Release, native architecture.
- `just build-all` — Debug and Release for x64, x86, and ARM64.
- `just run` — build Debug and launch the GUI.
- `just run6` — build Debug and launch pserv6 (elevated shell: it requires administrator).
- `just selftest` — build Release and run `pserv6 --selftest`, which loads, scrolls and refreshes every view and prints a timing log; `-RunAsInvoker` measures unelevated.
- `just run-cli services` — build Debug and list services through the CLI.
- `just --list` — current public recipes.

The `justfile` drives MSBuild against `pserv5/pserv5.slnx`, containing `pserv5/pserv6/pserv6.vcxproj` (the WinUI 3 GUI, x64 and ARM64 only, output `pserv5/bin/pserv6/<Platform>/<Configuration>/` with its self-contained WinAppSDK runtime; build recipe in Appendix A of `docs/pserv6-plan.md`), `pserv5/pserv5.vcxproj` (ImGui GUI), `pserv5/pservc/pservc.vcxproj` (CLI), `pserv5/pserv_core/pserv_core.vcxproj` (the shared core as a static library, linked by the GUI, the CLI and the tests) and `pserv5/tests/pserv_core_tests.vcxproj`. Build the solution: project paths depend on `$(SolutionDir)`. Output is `pserv5/bin/<Platform>/<Configuration>/`; the x86 solution platform uses `Win32` as the output directory name. Intermediates live under `pserv5/temp/`. Native recipes select ARM64 on ARM64 hosts and x64 otherwise.

Verify changes compile before handing off, and run `just test`: it builds Debug and runs `pserv_core_tests`, which checks every controller against live system data (column/property-id contract, typed values fit column types) and exits non-zero on any failure. It does not exercise the GUI or actions. For behavior changes, exercise the affected GUI or CLI path in addition to building and report what was actually checked. CLI help and listing commands provide read-only smoke checks, for example `pserv5/bin/x64/Debug/pservc.exe --help` (adjust for the built architecture). Build success alone does not verify UI behavior.

`just clean` removes all of `pserv5/bin/`, `pserv5/temp/`, and `dist/`; `just rebuild` performs that cleanup before building Debug. Prefer incremental builds unless cleanup is needed.

## Repository Map

Paths below are relative to `pserv5/` unless stated otherwise.

| Location | Responsibility |
| --- | --- |
| `pserv6/` | WinUI 3 GUI: `MainWindow` (navigation, toolbar, refresh coordinator on a worker), `data_grid` (virtualized `ListView` grid fed by the controllers), `self_test` (`--selftest`) |
| `pserv5.cpp`, `main_window.h/.cpp` | GUI startup, main window, table rendering and interaction |
| `pservc/` | CLI entry point, argument dispatch, console output |
| `core/` | Shared object, container, controller and action abstractions; async operations |
| `core/exporters/` | Export interfaces, registry, JSON and plain-text exporters |
| `models/` | Data items and their display/typed properties |
| `controllers/` | Enumeration, columns, filtering, sorting, available actions |
| `actions/` | Commands operating on selected objects |
| `windows_api/` | Windows resource enumeration and manipulation |
| `dialogs/` | GUI properties and theme-selection dialogs |
| `config/` | TOML-backed settings and per-view configuration |
| `theme/`, `themes/` | Base16 theme loader and YAML theme files |
| `utils/` | Logging, string conversions, Win32 errors, common app setup |
| Repository `docs/pserv5.md`, `docs/pservc.md` | User-facing GUI and CLI manuals |
| Repository `setup/`, `scripts/` | MSIS installers and version-stamping script |

## Architecture and Change Conventions

- Keep shared behavior in the model/controller/action and Windows API layers so both frontends benefit. The core (`pserv_core`) is compiled once for every frontend and must not depend on a UI toolkit or a frontend macro: actions never prompt (confirmation via `RequiresConfirmation`/`GetConfirmationText`, input via `GetParameters`, collected by the frontend before `Execute`), report non-fatal notes through `IActionHost` and throw on failure. `PSERV_IMGUI_FRONTEND` only adds ImGui/D3D headers to the GUI's precompiled header; do not introduce ImGui dependencies into shared code.
- `DataObject` supplies display properties, typed properties for sorting, filtering, and stable IDs. Keep column metadata and both property representations consistent when adding or changing columns.
- `DataController` defines columns and actions and refreshes its objects. `DataAction` executes through `DataActionDispatchContext`; preserve availability, destructive-action, and confirmation semantics in both frontends.
- Objects use intrusive `Retain`/`Release` reference counting. `DataObjectContainer::Append` takes the incoming reference; it does not retain it again. `GetByStableId` returns a borrowed pointer and marks the object seen in the current refresh generation. Preserve the `StartRefresh`/`FinishRefresh` lifecycle and check ownership when selections or async work outlive a refresh.
- Data refresh: a controller implements only `Collect(target, request)`, which runs on the refresh worker and may read nothing but `request`, `target` and immutable state. `Refresh()` (synchronous) or `RefreshCoordinator` (worker plus UI-thread `Publish`) do the rest; call `InvalidatePendingRefreshes()` on anything an older enumeration must not overwrite (committed edit, action, target change, view switch). UI-side derived state belongs in `OnPublished()`.
- Use `core/async_operation.h` for existing background-operation patterns, including progress, cancellation, and completion notification. Keep object and dispatch-context lifetimes valid until work completes.
- Follow adjacent code: four-space indentation, braces on separate lines, `pserv` namespaces, and existing `m_` / `m_p` / `m_b` member conventions. Translation units generally include `precomp.h` first.
- Use UTF-8 internally and the helpers in `utils/string_utils.h` for UTF-16 conversion at Windows API boundaries. Use existing WIL RAII types for Windows resources where appropriate.
- Use spdlog and `utils/win32_error.h` for diagnostics. Distinguish APIs reporting `GetLastError()` from APIs returning an error code directly; preserve the relevant error before other calls can overwrite it.

When adding a view:

1. Add the model, controller, Windows API wrapper, and actions as needed, following a comparable existing view.
2. Register the controller in `core/data_controller_library.cpp`; add its name constant in `core/data_controller.h`.
3. Add the per-view settings in `config/settings.h` and the lookup in `config/settings.cpp`.
4. Add source/header entries to the relevant `.vcxproj` files and maintain their `.filters` files. Shared sources go into `pserv_core/pserv_core.vcxproj`, which the GUI, the CLI and the tests link; they are not discovered automatically. List a controller's columns in its model's property-id order (`DataObjectColumn` takes the property enum first); `just test` fails otherwise.
5. Check CLI argument/action registration and behavior. The CLI enumerates the shared controller library, but frontend-specific action handling may still be required.
6. Update the relevant user manual when behavior or commands change.

## Dependencies and Themes

Dependencies are Git submodules: `imgui`, `spdlog`, `wil`, `tomlplusplus`, `rapidjson`, `argparse`, and `fkYAML`. Initialize or resynchronize them with `git submodule update --init --recursive` when needed, particularly after switching revisions. Avoid unrelated changes to vendored code or submodule revisions.

Configuration uses TOML; Base16 themes use YAML parsed with fkYAML. Edit theme sources in `pserv5/themes/`. The GUI project's `CopyThemes` target copies them to `themes/` beside the built executable; output copies are not source files.

## Versioning and Packaging

- `just version X.Y.Z` updates `pserv5/version.h` and `setup/*.msis`; resources consume the version header.
- Packaging requires MSIS 3.x (`msis` on PATH) in addition to the VS build tools.
- `just package-x64`, `just package-x86`, and `just package-arm64` build and stage the respective Release payload and symbols, then create an MSI. `just package-all` runs all three.
- `just bundle` combines the existing per-architecture MSIs; `just release` builds all packages and the bundle.
- Staging uses `dist/stage/<arch>/` and `dist/symbols/<arch>/`. Final MSI and bundle outputs land in `setup/`. Keep generated binaries and staging output out of commits.

Treat the current `justfile`, solution, project files, and implementation as authoritative when older documentation disagrees. In particular, the README's VS 2022 / `.sln` build instructions and the `justfile` introductory comment claiming packaging is absent are outdated.
