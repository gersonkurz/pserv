# pserv6

pserv6 is a native C++/WinRT / WinUI 3 Windows administration tool using Windows App SDK 2.7.0. The GUI and `pserv6c.exe` command-line frontend share controllers, models, actions and Windows API wrappers.

## Build, run and package

Use a Visual Studio 2026 Developer shell with v145 C++ tools, Windows SDK 10.0.26100.0, `just` and `nuget`. Initialize Git submodules with `git submodule update --init --recursive`.

```powershell
just build-release  # Native Release GUI and CLI; no launch
just run            # Build and launch native Release GUI
just run-cli services
just build          # Debug GUI and CLI
just run-debug
just test           # Six regression checks; use a native-target Developer shell
just package-all    # x64, x86, ARM64 MSIs AND the final bundle; requires MSIS 3.x
```

Open `pserv6/pserv6.slnx`; do not build individual projects without SolutionDir. Native architecture detection queries Windows and selects ARM64 even under an emulated shell. Explicit builds: `just build-winui-release x64`, `just build-winui ARM64`. `just build-all` builds Debug and Release for all three architectures.

Output: `pserv6/bin/<Platform>/<Configuration>/`; x86 uses `Win32`. Keep all adjacent runtime files with the executable. Windows App SDK and the matching v145 MSVC CRT are included beside the Release executables; no global runtime installation is required. Staging is under `dist/stage/<arch>/`, symbols under `dist/symbols/<arch>/`. Installers are under `setup/`; the current bundle is `pserv6.0.0.exe`. `just bundle` combines already-built MSIs. `just version X.Y.Z` updates code/resource/installer versions together.

## Views and table

The navigation pane offers Services, Devices (driver services), Processes, Windows, Modules, Installed programs, Environment variables, Startup programs, Network connections and Scheduled tasks.

- Search matches all item properties, case-insensitively. Ctrl+F focuses search.
- Ctrl+1 through Ctrl+9 select the first nine views; Ctrl+0 selects Scheduled tasks. Remote mode and active operations still restrict view switching. View shortcuts do not intercept text-field editing.
- Click a header to sort; click again to reverse. Sorting uses typed values, including numeric PIDs. Drag the header's right edge to resize.
- View → Columns selects visible columns and their order. At least one must remain visible.
- Ctrl/Shift select multiple rows. Ctrl+A selects all; Ctrl+C copies plain text. Edit also offers JSON copy. More actions and the row context menu expose file export and provider commands.
- Toolbar buttons use icons, tooltips and accessible names. Common commands, Elevate, Refresh and search share one row.
- Properties opens with its icon, double-click or Enter. Editable properties use controller metadata; Close discards edits and Apply changes submits changed fields. Read-only providers have no Apply button.
- Delete invokes an available Delete/Terminate Process action when the table has focus, using the normal confirmation flow. It does not delete objects while editing text or with a dialog open.
- Disabled services are grey. Running services use the Windows accent or Base16 green. Disabled state takes precedence over running.
- F5/Refresh reads a snapshot. The refresh toggle pauses/resumes automatic updates globally; Ctrl+R toggles it. Switching views preserves that preference. View → Refresh interval offers 1, 2, 5, 10, 30 or 60 seconds. Windows, Modules and Scheduled tasks use manual refresh by controller policy; their disabled toggle does not change the global preference.
- Refresh preserves row identity, selection and scroll position for surviving rows. The footer separates the last refresh time from elapsed snapshot time. Counts of highlighted/disabled items describe the complete provider, independently of search.
- Both scrollbars belong to the list; the vertical bar stays at the viewport edge. Narrow tables align left.

Environment variables offers Add variable for user or system scope. Existing names are rejected. Properties can rename and edit values: a rename preserves the source until the replacement exists, rejects collisions and preserves the registry string type. Failure to delete the source rolls back the new value where possible; rollback failure is reported rather than silently losing the source.

## Appearance

Themes → Dark themes / Light themes includes all **20 original bundled Base16 palettes**, plus Windows Dark/Light and System. YAML sources are in `pserv6/themes/`; builds copy them beside the executable. Additional valid YAML schemes in that directory appear on the next launch.

View → Font opens the native Windows font dialog, including proportional fonts and a sample preview. Apply previews the font throughout the application; OK saves it, Cancel restores the previous appearance. Font family, size, weight and italic style apply to text; icon glyphs retain their icon font. The OS title bar and native system dialogs use Windows appearance settings.

Ctrl+mousewheel or Ctrl+plus/minus scales the whole interface (8–32 base font-size range); Ctrl+Shift+0 or Reset zoom restores the default size. The plus/minus shortcuts also support the numeric keypad and leave text-field editing alone. Theme, font, zoom, window placement, active view, global automatic-refresh preference/interval and each view's search/sort/column layout persist in `%LOCALAPPDATA%/pserv6/appearance.toml`. When migrating older per-view refresh settings, an explicit pause in any automatic-refresh-capable view becomes a global pause.

## Permissions and remote computers

pserv6 starts with the current Windows permissions. Elevate opens a separate local administrator window through UAC, retaining the current window. The button is hidden when already elevated. Protected services may provide only partial data without administrator access.

File → Connect to remote machine connects Services and Devices using current Windows credentials. Connection is checked asynchronously. Other providers are disabled remotely because their APIs are local-only. File → Connect to local machine restores them. Service actions and property edits carry the selected machine to SCM; local-path/registry actions are hidden remotely.

Destructive actions retain shared confirmation semantics. Background actions show progress and support cancellation where implemented by their shared action; cancellation cannot undo completed changes. Help offers About and the bundled manual, opened through the Windows Markdown association.

## Implementation and diagnostics

`pserv6/ui/` owns XAML and presentation; `pserv6/cli/` owns console output. Shared code has no UI-framework dependency. Enumeration uses `core/async_operation.h` and a separate controller snapshot. Publication reconciles rows by stable ID; unchanged snapshots produce no collection changes. Workers never mutate displayed objects. Snapshot publication waits during dialogs/actions and discards stale results after actions or provider changes. A virtualized ListView renders the table.

Diagnostics: `%LOCALAPPDATA%/pserv6/winui.log`. The CLI supports its own logging options; see [CLI manual](pserv6c.md). The [release review](release-review.md) records the changes, verification and remaining validation limits. A successful build alone does not prove every administrative workflow or deployment environment.
