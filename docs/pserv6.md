# pserv6 — WinUI frontend

`pserv6.exe` is the new WinUI 3 application, using Windows App SDK 2.7.0 and native C++/WinRT. It connects all ten local providers to the shared table, properties and action interfaces. The pserv5 ImGui application and CLI remain available through the explicit legacy build recipes. All ten views have passed a read-only ARM64 Release smoke check; this does not validate every action or deployment environment.

## Build and run

Use a Visual Studio 2026 Developer shell with the v145 C++ tools, Windows SDK 10.0.26100.0, `just`, and `nuget` on PATH. Initialize the repository submodules first if this is a fresh checkout.

```powershell
just build-release  # Build native Release without launching
just run            # Build and launch native Release
```

`just` queries Windows with `IsWow64Process2` to select the native architecture, including ARM64 when the shell or `just` is emulated. `just build` builds pserv6 Debug; `just run-winui` builds and launches Debug. Explicit cross-builds remain available through `just build-winui-release x64` or `just build-winui ARM64`. Build the solution `pserv5/pserv6.slnx`, not the project alone: output paths depend on `$(SolutionDir)`.

On ARM64, the Release executable is `pserv5/bin/ARM64/Release/pserv6/pserv6.exe`. Keep its adjacent runtime files together. `just run-legacy` launches the old ImGui GUI, `just build-legacy` builds it with the CLI, and `just run-cli services` invokes the CLI.

The recipes restore the pinned `packages.config` dependencies to the ignored `pserv5/pserv6/packages/` directory. Initial restore requires access to NuGet. Build output is under `pserv5/bin/<Platform>/<Configuration>/pserv6/`, and intermediates are under `pserv5/temp/pserv6/`.

This is an unpackaged desktop application with the Windows App SDK runtime copied beside it. Distribute the **whole output directory**, not just the executable. The MSVC runtime is dynamically linked and must also be installed on the target machine. Existing MSI recipes do not yet package this frontend.

## Views

The navigation pane offers Services, Devices (driver services, not a hardware device tree), Processes, Windows, Modules, Installed programs, Environment variables, Startup programs, Network connections, and Scheduled tasks.

Each view uses its existing controller's columns, typed sorting, filtering, visual states and actions. All columns are accessible by horizontal scrolling. Search, column widths, sort order and live-refresh preference are remembered separately for each view during the session. Windows, Modules and Scheduled tasks use manual refresh, matching their shared controller policy.

Environment variables also provides an Add variable dialog for user or system scope. Existing names are rejected in the selected scope; edit existing variables through Properties. System changes require suitable Windows permissions.

## Table and actions

- Search filters all item properties through the shared controller's case-insensitive matching.
- Click a column header to sort; click again to reverse. PID uses numeric sorting. Drag a header's right edge to resize its column.
- Disabled services use grey text; running services use the Windows accent colour (Base16 green when a palette is selected); stopped services use normal text. Disabled takes precedence over running, as in the ImGui frontend.
- Ctrl+mousewheel scales the interface, using the original 8–32 font-size range; Ctrl+0 restores the default. Zoom is session-only.
- Ctrl/Shift selection supports multiple items. Actions must be available for every selected item; individual actions retain their existing behavior, including actions that operate on the first selection only.
- Common actions appear on each view's toolbar. More and the row context menu expose the shared actions, including their existing destructive-action confirmations. Some shared actions still use native Win32 dialogs or open external Windows tools.
- Properties (also double-click or Enter) shows all fields and uses the controller's edit metadata. For Services, display name, startup type, binary path and description are editable. Read-only views have no Apply button. Close discards edits; Apply changes submits changed fields.
- Refresh or F5 reads a new snapshot. Live refresh runs every five seconds and can be paused. Existing row objects and the item source are retained; only changed properties and added/removed/reordered rows notify the list. An unchanged refresh preserves selection, focus and scroll position without rebuilding the table or changing its height.
- Theme offers System, Light, Dark and the original Base16 YAML palettes from the adjacent themes directory. Theme and table font persist in %LOCALAPPDATA%/pserv6/appearance.toml; column widths, search and zoom remain session-only.
- View → Font selects an installed table font. Ctrl+mousewheel still scales the entire interface.
- Edit → Copy as Plain Text / Copy as JSON copies the selected rows; Ctrl+C copies plain text. The More icon and row context menu also offer file export in both formats.
- The list owns both scrollbars, keeping the vertical bar at the viewport edge. Short tables align left. The footer separates the last refresh time from the duration of that refresh.
- Help opens the About dialog and the bundled documentation. The Markdown manual opens in the Windows-associated application.

Changes use the process's current Windows permissions. The Elevate button opens a separate local administrator window through the Windows UAC prompt; the current window remains open. The button is hidden when already elevated. Background actions report progress and support the shared cancellation behavior; cancellation does not undo completed changes. The footer counts highlighted and disabled/dimmed items across the entire view, independently of the search filter.

## Local and remote computers

File → Connect to remote machine connects Services and Devices to the named computer using the current Windows credentials. The connection is checked in a background operation before switching views. The remaining providers are disabled in remote mode because their APIs are local-only. File → Connect to local machine restores all views.

Service control, startup-type changes, uninstall and service property edits carry the selected machine to the Service Control Manager. Actions that open local paths, Registry Editor or delete registry keys are unavailable remotely. Remote operation requires network access and appropriate Windows permissions.

## Implementation and diagnostics

The frontend compiles the existing controllers, models, actions and Windows API wrappers. `PSERV_WINUI_BUILD` excludes the ImGui-specific interfaces from that build. Enumeration runs through `core/async_operation.h` into a separate controller; the UI reconciles the completed snapshot by stable object ID, retaining row identities and notifying bindings only when values change. Enumeration and property-edit workers never mutate displayed objects. A virtualized ListView renders the table. Snapshot publication waits while a properties dialog or action is active; a snapshot taken before an action is discarded and refreshed afterwards. Switching providers also discards pending results from the previous view.

Diagnostics are written to `%LOCALAPPDATA%\pserv6\winui.log`, with rotating backups. Some protected services expose only partial information under standard access; Windows API errors are logged.

The shared service-editing regression check uses a fake Service Control Manager boundary and does not modify system services:

```powershell
.\scripts\test-service-edits.ps1
.\scripts\test-winui-rows.ps1
.\scripts\test-winui-cell-collection.ps1
```

It checks the display-name column mapping, rejection of service identity edits and invalid startup types, propagation of configuration failure, model updates after success, and propagation of the selected remote machine to the SCM boundary.

The row reconciliation check covers unchanged snapshots (zero collection events), additions, removals, reordering, empty results and retained object identity.

The cell-collection check builds the actual `DataRow` and verifies the `IVector<IInspectable>` and `IIterable<IInspectable>` interfaces required by XAML `ItemsSource`, including access to the same underlying contents. It requires generated headers from an ARM64 Release build and a matching Developer shell by default; `-Platform` and `-Configuration` can select another existing build. It does not start the application or enumerate system objects. A plain `single_threaded_vector<DataCell>` does not expose these interfaces; the observable vector factory does.

## Current scope

MSI integration, persistent per-view layouts and full workflow parity are still unfinished. Only appearance settings persist so far. Remote support is limited to services and drivers. Elevation is explicit; startup does not automatically request administrator access. Provider integration alone does not make this a validated replacement for every workflow in `pserv5.exe`.

## Review fixes, 2026-11-18

The review pass adds the original icon, Base16 palettes, menu/remote connection, clipboard and file export, About/documentation, table font choice and explicit elevation. It also revises table scrolling/alignment and refresh wording. The ARM64 Release build and original ARM64 Debug GUI/CLI build compile. Fake-SCM service-edit checks and row reconciliation checks pass. Runtime checks on this ARM64 machine:
- Original icon visible; menu and About dialog open.
- Services retains both scroll offsets with a selected row off-screen through repeated live refreshes. Headers track horizontal scrolling; the vertical bar remains at the viewport edge.
- Modules loads over 10,000 rows and aligns the narrower table to the left.
- Catppuccin Mocha applies and survives restart. Selecting Lucida Sans Typewriter changes table cells and is saved.
- Ctrl+C copies the selected service as plain text. Edit → Copy as JSON produces parseable JSON with that service's fields.
- The remote menu connects using this computer's name, lists 293 services, disables local-only providers, and switches back to local mode successfully.
- All three regression scripts pass, including the actual DataRow/XAML collection check. The rebuilt ARM64 CLI Services help and filtered JSON listing pass.
- No new unhandled WinUI exceptions were recorded during these checks.

A separate remote host, live service mutations, the UAC elevation round-trip and file export have not been exercised. The documentation launcher uses the Windows Markdown file association. These limits matter: this remains a development build, not a feature-complete replacement.

## Validation on the development machine

Current integration build checks (2026-11-18):

- WinUI Release for ARM64 and x64, including all ten providers and generated XAML bindings.
- Original ImGui and CLI Debug for x64 after the shared-code changes.
- No application or test executable was launched during the initial integration pass, at the user's request. The later authorized runtime checks are described below.

After a reported startup crash, the row cell collection was corrected to expose the XAML ItemsSource interfaces. ARM64 Release rebuilt successfully, and the isolated actual-DataRow collection check passed.

With permission to resume runtime testing, the corrected ARM64 Release started successfully and remained responsive through all ten views, returning to Services afterwards. No new application crash events or unhandled WinUI exceptions were recorded during this run. Observed results on this machine:

| View | Rows | Displayed snapshot time |
| --- | ---: | ---: |
| Services | 293 | typically 106–139 ms; some refreshes around 395 ms |
| Devices (driver services) | 321 | 111 ms |
| Processes | 320 | 395 ms |
| Windows | 156 | 341 ms |
| Modules | 10,015 | 2,444 ms |
| Installed programs | 1,093 | 115 ms |
| Environment variables | 37 | 23 ms |
| Startup programs | 12 | 45 ms |
| Network connections | 157 | 55 ms |
| Scheduled tasks | 204 | 588 ms |

These are individual UI-reported observations including polling and publication, not controlled benchmarks. Counts change with machine activity. Services filtering, grey disabled rows (12 disabled services), selection preservation across live/manual refresh, and opening/closing service properties were checked. The environment Add variable dialog opened and cancelled successfully. No system-changing actions or property edits were applied; those workflows still need validation.

Earlier checks on the Services-only version:

- WinUI Release builds for ARM64 and x64, and Debug for ARM64.
- Original ImGui and CLI Debug build for x64; CLI Services help and a filtered JSON listing.
- Service-editing regression checks described above.
- Native ARM64 UI: real enumeration (293 services), filtering, selection, sorting, selection preservation across refresh, mixed-state multi-selection and action availability, column resizing, light/dark rendering, and opening/scrolling/closing properties without applying changes.

Earlier Services refresh-to-display times were approximately 125–200 ms on this machine. These include the completion polling interval and do not establish performance for the generic table or other providers. Live service mutations, installer deployment and other Windows installations have not been validated. Builds report existing unused-parameter/local and shadowing warnings in shared provider code.
