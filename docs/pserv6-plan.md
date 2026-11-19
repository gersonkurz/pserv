# pserv6 — WinUI 3 frontend: plan

**Status:** draft, revision 2 (after review round 1), 2026-11-18. Nothing implemented.
**Supersedes:** `docs/pserv6-migration-plan.md` (Direct2D / `pnq::ui`, approved 2026-09-18),
voided by the owner on 2026-11-18. None of its decisions carry over unless restated here.

---

## 1. Goal and hard requirements

Replace pserv5's Dear ImGui frontend with a **WinUI 3** frontend, keeping the shared core and
the CLI (`pservc.exe`). Owner requirements:

1. **WinUI 3.** Native C++/WinRT; no .NET.
2. **Blazingly fast.** Turned into budgets in section 7.
3. **Must work.** Previous AI attempts at WinUI 3 failed on build complexity. Every phase ends
   on an *executed* check, the installer exists from Phase 3 on, and the build recipe in
   Appendix A is the one that was actually built and run.
4. **MSI-installable** through MSIS (`C:\NGBT\MSIS\msis3`), like pserv5.
5. **The product is called pserv6 everywhere**: product name, install folder, executable,
   Start-menu entry, settings folder (owner, 2026-11-18).

Non-goal: changing what the ten views show or what their actions do, beyond the bug fixes named
here.

---

## 2. Evidence: what has been executed

Two throwaway probes, built and run on the dev box on 2026-11-18. Sources lived in the session
scratch directory and are **not** in the repo; Appendix A records the recipe.

**Dev box:** Windows 11 Pro 26300 on **ARM64** (Parallels), VS 2026 18.12.1 (toolset v145,
`stdcpplatest`), Windows SDK 26100, Windows App SDK (WinAppSDK) 1.8 (WinUI `1.8.260224000`,
Foundation `1.8.260222000`), C++/WinRT `2.0.250303.1`, MSIS 3.0.9 built from source. Nothing had
to be installed.

### 2.1 Probe 1 — toolchain and installer

A XAML app with IDL, `x:Bind`, a C++/WinRT runtime class with change notification, and a
20,000-row virtualized `ListView`. Unpackaged and self-contained.

| Check | Result |
|---|---|
| Build: Debug/Release × ARM64/x64, from the command line with `msbuild -restore` | all succeed, **0 warnings**, ~40 s each from clean |
| Incremental Release (LTCG) after touching one `.cpp` | 23 s |
| First frame from process creation, ARM64 Release, 5 runs | 150–295 ms (≈150 ms warm) |
| Working set | ~118 MB |
| x64 build under ARM64 emulation | runs; first frame ~214 ms warm, ~950 ms cold |
| Payload without `.pdb/.lib/.exp/.winmd` | 211 files, **53.5 MB** (86 of the folders are WinUI localization) |
| MSI built with MSIS 3.0.9 `/STANDALONE` | 14.5 MB, built in 14 s |
| `msiexec /a` extract vs staged payload | identical: 211 files, 87 folders |
| Launched from the extracted install layout, 3 runs | first frame 143–268 ms; no `.winmd` needed at runtime |
| `msis-check-msi.ps1` | "OK: changes no Windows-owned registry key" |

**Not executed:** a real per-machine install into Program Files plus uninstall. Phase 3 closes
this.

### 2.2 Probe 2 — the real pserv core inside WinUI

All pserv core sources (everything `pservc.vcxproj` compiles except `pservc/*`) built straight
from the repo as a static library in its CLI flavour (`_CONSOLE`). A WinUI app links it through
a narrow bridge translation unit and shows any of the ten controllers in a grid: columns built
in code at runtime, `ListView` virtualization, a refresh diff by object identity, and in-place
text updates of realized rows only. An unattended stress mode (`PROBE_AUTORUN`) loads each view,
scrolls on every frame for 12 s with the 2 s auto-refresh running, and logs frame intervals.

- **The core compiles and links unchanged.** It needed `NOMINMAX` in the WinUI PCH, and the
  classic Win32 import libraries restored to the link (F7).
- Builds: ARM64 Release, ARM64 Debug and x64 Release, 0 warnings.
- **Elevated:** a `requireAdministrator` build (manifest verified with `mt.exe`) was launched
  by the owner through UAC; it started, rendered and worked. The autorun did not run in that
  launch, because environment variables do not cross the UAC boundary, so **there are no
  elevated timings**.

**Probe revisions.** The first all-view run (`run1`) had a defect: its in-place row update
found no realized rows (`realized=0` on every tick), so it never exercised the update path. Its
numbers are discarded. The table below is the **corrected** probe (`run4`): realized-row
updates touched 10–37 rows per tick across 60 ticks. It measures frames only after the
post-bind frame plus 500 ms, and attributes every frame over 33.4 ms by checking whether a
refresh tick ran inside that frame interval.

ARM64 Release, **not elevated** (several columns stay empty without admin rights):

| View | Rows | Refresh tick p50 / max (core + diff + update), UI thread | Frame p50 / p95 / max | Frames > 33 ms: during a tick / not | Bind → first frame |
|---|---:|---:|---|---|---:|
| Services | 293 | 72.8 / 80.1 ms | 8.3 / 9.6 / 106 ms | 6 / 0 | 114 ms |
| Devices | 321 | 71.1 / 81.4 ms | 8.3 / 9.2 / 97 ms | 6 / 0 | 569 ms |
| Processes | 309 | 25.2 / 26.0 ms | 8.3 / 9.2 / 44 ms | 6 / 0 | 289 ms |
| Windows | 148 | 4.3 / 4.5 ms | 8.3 / 9.2 / 27 ms | 0 / 0 | 216 ms |
| Modules | 9,832 | 1,829 / 1,961 ms | 8.3 / 9.3 / 1,891 ms | 4 / 0 | 1,975 ms ¹ |
| Uninstaller | 1,093 | 49.8 / 53.8 ms | 8.3 / 9.2 / 70 ms | 5 / 0 | 198 ms |
| Environment Variables | 37 | 0.6 / 0.8 ms | 8.3 / 9.3 / 22 ms | 0 / 0 | 98 ms |
| Startup Programs | 10 | 2.5 / 3.7 ms | 8.3 / 9.0 / 34 ms | 1 / 0 | 100 ms |
| Network Connections | 139–149 | 2.2 / 4.1 ms | 8.3 / 16.7 / 187 ms | **0 / 10** | 147 ms |
| Scheduled Tasks | 204 | 413.6 / 697.1 ms | 8.3 / 9.7 / 717 ms | 5 / 0 | 598 ms ¹ |

¹ "Bind → first frame" runs from setting `ItemsSource` to the next rendered frame, so it
includes any refresh tick that fired in between. For the two views marked ¹, `run4.log` records
a tick between bind and frame, and the tick alone takes about as long as the gap. Devices
records **no** tick there: its initial enumeration (215 ms) ran before binding, so its 569 ms
is unexplained.

What this does and does not show:

1. **The refresh tick runs synchronously on the UI thread**, so a frame that contains a 70 ms
   tick cannot be shorter than 70 ms. In 9 of 10 views, every frame over 33 ms overlapped a
   tick. Moving the refresh off the UI thread (D5) removes that cause.
2. **Network Connections had 10 long frames (max 187 ms) that overlapped no tick.** Its ticks
   take 2–4 ms but insert and remove rows (2–13 collection events). One candidate is `ListView`
   layout deferred to the frame after the collection change, which a time-overlap test cannot
   attribute. **Unexplained.** Phase 4 investigates it with a WPR trace before the grid is
   declared done.
3. **A view switch takes 98–569 ms** from bind to painted frame in the views with no tick in
   between (worst: Devices, 569 ms), which is noticeable. Not yet analysed. One candidate is the probe rebuilding every recycled
   container's cells when the column set changes.
4. **Not measured:** the proposed `Publish` merge (it does not exist yet), and layout/render
   cost after binding ~10k rows separately from the Modules tick. The 6.5 ms Modules
   `bind_ms` is collection construction only.
5. The UI-side update of realized rows measured 0.3–1.3 ms per tick, and the identity diff
   0.0–0.2 ms.

Startup: first frame 297 ms in this run (Services loaded synchronously first). Working set
123–138 MB, against single-digit MB for pserv5's binary; that is the price of WinUI, stated
rather than hidden.

### 2.3 Defects and traps found along the way

| # | Finding | Where it goes |
|---|---|---|
| F1 | **pserv5 bug:** the Processes columns from index 9 on do not match `ProcessProperty`. "Handles" shows the parent PID, "Start Time" shows peak working set, and so on; `controllers/processes_data_controller.cpp:16-32` vs `models/process_info.h:12-37`. Affects the shipped GUI and CLI, display and sorting. | Phase 0 |
| F2 | `FinishRefresh` restarts its scan after every removal (`core/data_object_container.cpp:107-129`), so it is quadratic when many objects disappear at once. | Phase 2 |
| F3 | `ApplicationType=Windows Store` makes NuGet restore assume UAP and use the legacy asset resolver. Needs `NuGetTargetMoniker` and `RuntimeIdentifiers`. | Appendix A |
| F4 | WinAppSDK 1.8 Foundation adds `WindowsAppRuntimeAutoInitializer.cpp` to `ClCompile` twice (MSB8027/LNK4042). Root cause not found; contained with a de-dup target. | Appendix A |
| F5 | The `Microsoft.WindowsAppSDK` meta-package pulls in ONNX Runtime and DirectML (~38 MB). Reference the component packages instead. | Appendix A |
| F6 | `ApplicationType=Windows Store` leaves `EnableUAC` empty, which **silently drops `UACExecutionLevel`**: an "elevated" build had no `requestedExecutionLevel` in its manifest. | Appendix A, Phase 3 check |
| F7 | `MinimalCoreWin=true` drops the classic Win32 import libraries, so the core fails to link (`ShellExecuteW`, `SHGetFolderPathW`). | Appendix A |
| F8 | pserv5's `.msis` scripts set `REMOVE_FOLDERS_ON_UNINSTALL=True` without `USE_INSTALLER_HOOKS=True`. MSIS 3.0.9 warns that this has no effect. | Phase 0 |
| F9 | `C:\Program Files\MSIS` is 3.0.6, inside the 3.0.0–3.0.7 registry-permissions range. pserv imports no `.reg` file, so it is not affected. Needed before Phase 3: 3.0.9. | Phase 3 prerequisite |
| F10 | MSIS's ARM64 template triggers WiX `WIX1143` (InstallerVersion < 500, corrected automatically). | report upstream (msis) |
| F11 | Environment variables do not reach an elevated process started through UAC, so a self-test configured by environment never runs elevated. | `--selftest` takes command-line arguments |

---

## 3. Decisions

| # | Decision | Rationale |
|---|---|---|
| **D1** | **WinUI 3 with C++/WinRT, Windows App SDK pinned to 1.8, unpackaged, self-contained.** | Owner decision. Unpackaged and self-contained means an xcopy payload: no MSIX, no framework package or runtime installer, and MSIS can install it like any other files. Pinned because WinAppSDK changes build targets between releases (F3–F5); upgrades are deliberate, single-purpose changes. |
| **D2** | **Component packages, not the meta-package**: `Microsoft.WindowsAppSDK.WinUI` and `.Foundation` (F5). | Saves ~38 MB of ML runtime pserv never calls. |
| **D3** | **One core library, one flavour.** The shared code becomes a static library `pserv_core`, compiled **once**, with no `PSERV_CONSOLE_BUILD` and no ImGui. `pservc`, the ImGui GUI (until it is removed) and `pserv6` all link it. | Today the macro changes **class layout and vtables** (`DataController` has a dialog member and different virtual functions per flavour; `core/data_controller.h`). Two flavours of one class in one process is an ODR violation waiting to happen. Probe 2 worked around it with the CLI flavour; production must not. |
| **D4** | **Frontend services go through an interface, not `#ifdef`, and `Execute` never prompts.** Interaction that needs the user happens in the frontend **before** dispatch: confirmation (`RequiresConfirmation()` plus a new `GetConfirmationText(selection)`), and parameters such as an output file (a new `GetParameters()`; the CLI satisfies them from arguments, e.g. `--output`). `Execute(ctx)` receives the answers in `ctx` and only uses non-interactive host services (`SetClipboardText`, `ReportError`, `ReportProgress` through `AsyncOperation`). | Today 50 files branch on `PSERV_CONSOLE_BUILD`, and the GUI branches call `MessageBoxA`, `ImGui::SetClipboardText` and an ImGui modal inside a controller; some actions branch on a synchronous confirmation, e.g. `actions/process_actions.cpp:178`. WinUI's `ContentDialog::ShowAsync` is asynchronous, and blocking on it on the UI thread is not allowed. Asking before dispatch keeps the core free of WinRT and of async types. **Lifetime:** the frontend builds `ctx`, `Retain`s every selected object when the dialog opens, and `Release`s after `Execute` (or its `AsyncOperation`) completes or is cancelled, so neither a refresh nor a view switch can free them. |
| **D5** | **Refresh off the UI thread, double-buffered, with explicit invalidation.** Each controller keeps a worker-owned *shadow* container. A worker runs the existing manager code against the shadow (Start/Enumerate/FinishRefresh, unchanged). The UI thread then **publishes**: it merges the shadow into the live container by stable ID (assign into existing objects, clone new ones, drop missing ones) and re-sorts. One refresh worker for the whole app. Rules in section 5.1. | Probe 2: in 9 of 10 views every frame over 33 ms overlapped the synchronous refresh tick. Managers carry no state across refreshes except process-wide statics (`s_moduleCache` in `module_manager.cpp`, the services error-log set), so they can run unchanged on a worker, *provided only one refresh runs at a time*. The UI only ever reads objects the UI thread owns, so there are no locks in the paint path. Rejected: a reader/writer lock (the UI would block for the whole refresh), and splitting every manager into collect/apply (touches all ten managers for the same result). `pservc` calls collect plus publish inline, so behaviour is unchanged. |
| **D6** | **The grid is our own control on top of `ListView`.** Columns are generated from `GetColumns()` at runtime, rows are filled in `ContainerContentChanging`, refreshes update realized rows in place, and the header supports sort, resize, reorder and per-view persistence. | WinUI 3 ships no DataGrid, and the Community Toolkit DataGrid was not carried forward. `ListView` provides virtualization, selection, keyboard handling and row-level UI Automation. Probe 2 ran this design on all ten views (section 2.2); its open items (Network Connections frames, view-switch latency) are Phase 4 work. Rejected: `ItemsRepeater` (we would reimplement selection, keyboard and UIA), and drawing the grid with Direct2D inside a `SwapChainPanel` (no accessibility at all). |
| **D7** | **The installer exists from Phase 3 on** and is rebuilt and installed at every later phase exit. | The "must work" requirement: packaging is where WinUI attempts traditionally die late. |
| **D8** | **`pservc` stays a plain console exe** linking `pserv_core`: no WinAppSDK, no XAML, still a few MB. | It is used in scripts and over SSH; it must not inherit a 54 MB runtime or start slowly. |
| **D9** | **Elevation as in pserv5**: `requireAdministrator`, applied through `EnableUAC=true` (F6), and verified by **reading the manifest out of the built exe** in `just test`. | pserv5 requires admin; F6 shows the setting can silently vanish. Owner observed the elevated probe working (section 2.2). |
| **D10** | **No x86.** pserv6 ships x64 and ARM64. *(owner, 2026-11-18)* | Windows 11 has no 32-bit edition, and Windows 10 reached end of support in October 2025. |
| **D11** | **Fluent theming only** (Light / Dark / follow Windows, plus the Windows accent colour). Base16 YAML themes, `theme/` and fkYAML go in Phase 7. *(owner, 2026-11-18)* | WinUI controls read dozens of theme resource keys; a Base16 mapping would cover the grid and leave menus and dialogs half-themed. |
| **D12** | **One MSI lineage, no preview MSIs.** Every pserv6 MSI, from the first one, uses **pserv5's per-arch UpgradeCode** (`672E9734-DC0A-4FD9-B25E-8544C621B230`), and the pserv6 bundle uses pserv5's bundle UpgradeCode. Product name and `INSTALLDIR` are `pserv6`; `ADD_TO_PATH=True` as in pserv5. Before GA, pserv6 runs **side by side** with an installed pserv5 straight from its build output or staged folder (xcopy-runnable, shown in 2.1), not from an MSI. *(owner accepted side-by-side then replace, and the pserv6 name, 2026-11-18; the single lineage is the review-round-1 correction)* | A preview product with its own UpgradeCode cannot be removed by a GA that uses pserv5's code: a standard major upgrade only finds its own UpgradeCode, and MSIS has no setting for removing a second one. MSIS also derives file-component GUIDs from UpgradeCode plus destination (`msis3/docs/overview.md`, Component ID Generation), so two lineages owning `pserv6\` would hold overlapping files under different identities, and uninstalling one could remove the other's payload. One lineage removes the preview-to-GA transition entirely. **Consequence:** installing *any* pserv6 MSI upgrades pserv5 away on that machine, so MSI tests run in a disposable environment (section 9). |
| **D13** | **Settings in `%APPDATA%\pserv6\config.toml`.** On first run, import window, view, auto-refresh, column widths/order/sort and the remote machine name from pserv5's file when present. Font size and theme do not carry over (D11). *(owner, 2026-11-18)* | pserv5 keeps working with its own file while both exist. |

## 4. Open decisions

None. O1–O4 of revision 1 were settled by the owner on 2026-11-18 and are now D10–D13.

---

## 5. Architecture

```
pservc.exe (console) ──┐
                       ├── pserv_core.lib  (models, controllers, actions, windows_api,
pserv6.exe (WinUI 3) ──┘                    config, exporters, utils; one flavour)
   └─ implements IActionHost, owns the UI thread, the refresh worker, the grid
```

**Seam (D3, D4).** `pserv_core` exposes:

- `DataController`: columns, actions, sort, the property-edit transaction (exists already); and
  the refresh split `RefreshShadow(const RefreshRequest&)` (worker) / `Publish()` (UI thread),
  with `Refresh()` doing both for the CLI.
- `DataController` loses `m_pPropertiesDialog`, `ShowPropertiesDialog`,
  `RenderPropertiesDialog` and `HasPropertiesDialogWithEdits`. Frontends own their dialogs.
- `DataAction`: `RequiresConfirmation()` (exists), plus new `GetConfirmationText(selection)`
  and `GetParameters()` (D4). `Execute(ctx)` reads answers from `ctx` and never prompts.
  "Add Environment Variable" is an action with two text parameters, replacing the ImGui modal
  that lived inside `EnvironmentVariablesDataController`. (Revision 2 planned a separate create
  transaction on the controller; implemented in Phase 1 as parameters instead, which is one
  mechanism instead of two and also gives the CLI `--name`/`--value`.)
- CLI argument registration (`RegisterArguments`, argparse) moves into `pservc`, which reads
  `GetColumns()` / `GetAllActions()` / `GetParameters()`. `GetAllActions()` becomes
  unconditional, so the core has no argparse dependency.

### 5.1 Refresh rules (D5)

- **Immutable inputs.** A refresh is started with a `RefreshRequest` value: view, target
  (remote machine name, service type filter, anything else a manager reads), the view
  generation, and the controller's **mutation epoch** at request time. The worker reads only
  the request, never controller fields the UI thread can change (today, for example,
  `ServicesDataController::m_machineName`). If the request's target differs from the target
  the shadow was last filled for, the worker clears the shadow first.
- **Mutation epoch.** The UI thread increments a controller's epoch when it commits a property
  edit, commits a create, starts or completes an action on that view, or changes the target
  (connect to local/remote machine). Edits keep updating live objects directly, as today
  (`services_data_controller.cpp:216`).
- **Publish rule.** A finished refresh is published only if its generation equals the current
  view generation **and** its epoch equals the controller's current epoch. Otherwise it is
  discarded and, unless auto-refresh is paused, a new refresh is requested. A refresh started
  before a mutation can therefore never write pre-mutation values over it.
- **Open dialogs.** While a properties dialog, create dialog, confirmation or `AsyncOperation`
  is open on the view, finished refreshes are discarded and no new auto-refresh starts
  (pserv5's `PauseDuringActions` / `PauseDuringEdits`). On close, one refresh is requested.
- **One worker, one in flight per controller**; F2 is fixed in the merge.
- **Completion** posts to the UI thread through `DispatcherQueue`; `AsyncOperation` completion
  moves from `WM_ASYNC_OPERATION_COMPLETE` to the same mechanism.

### 5.2 pserv6 shell

- `NavigationView` with the ten views, `Ctrl+1`…`Ctrl+0`.
- `MenuBar` with File (Refresh F5, Connect local/remote, Exit), View and Help.
- A filter box and a status line (item count, last refresh time, refresh duration).
- A context menu (`MenuFlyout`) built from `GetActions(selection)`, separators included;
  destructive actions styled; confirmation and parameter dialogs as `ContentDialog`s before
  dispatch (D4).
- A properties dialog generated from column metadata (`ColumnEditType` → `TextBox`, multiline
  `TextBox`, `NumberBox` or `ComboBox`) with the action buttons that have `PropertiesDialog`
  visibility.
- Progress through a `ContentDialog` with `ProgressBar` and Cancel.
- `Ctrl`+mouse wheel scales the grid font. `ExtendsContentIntoTitleBar` with Mica.
- `--selftest <views> --log <file> [--seconds N]`: command-line arguments, not environment
  variables (F11).

**Repository layout.** No mass file moves; history stays readable.
- Add `pserv5/pserv_core.vcxproj`, referencing today's source paths.
- Add `pserv6/` (the WinUI project) to `pserv5/pserv5.slnx`.
- The ImGui GUI (`pserv5.vcxproj`) keeps building, linked against `pserv_core`, until Phase 7
  deletes it.

---

## 6. Phases

Each phase is one or more reviewed commits (`yaaadabi` loop). An exit criterion is something
**executed**, never "compiles". `just test` is introduced in Phase 0 and grows every phase.
x64 is built in every phase. It is checked for function under emulation on the ARM64 dev box,
and its performance is assumed to match ARM64 (owner, 2026-11-18; R4).

### Phase 0 — Groundwork (no UI change)
- Fix **F1**, and make the contract behind it explicit: every `DataObjectColumn` names its model
  property enum (`DataObjectColumn{ProcessProperty::HandleCount, "Handles", ...}`), and column
  *i* must name property *i*, because every consumer reads properties by column index.
  `DataController` logs violations at construction.
- Add `pserv_core_tests` (a console exe, `just test`):
  - the contract holds for all ten controllers;
  - the contract check itself flags a deliberately misordered controller;
  - on live data, typed values fit the column type, and string columns display their typed
    value.

  Value agreement alone would **not** catch F1: display and typed values both read through the
  same misordered enum and agree with each other.
- Introduce `pserv_core` as a static library already in Phase 0, in the CLI flavour, linked by
  `pservc` and the tests; this avoids a third copy of the source list. Phase 1 removes the
  flavour and links the GUI too.
- Fill the **Loop parameters** block in `CLAUDE.md`. Verify: `just build-all && just test`.
  Yardstick: this plan plus `AGENTS.md`. Focus: core/UI threading, object lifetime, installer
  payload.
- `justfile`: add `-restore` to `_msbuild`; fix **F8** by dropping the no-op setting.
- **Exit:** `just build-all` and `just test` pass. The contract check fails on the pre-fix enum
  order and passes after; this is demonstrated in the review handover.
- **Deferred to Phase 3:** measuring pserv5 on the section 7 metrics. pserv5 requires elevation
  (an owner UAC click) and has no self-test mode; it will be measured alongside pserv6's first
  elevated `--selftest`.

### Phase 1 — `pserv_core`, one flavour (D3, D4)
- Introduce `IActionHost`, `GetConfirmationText` and `GetParameters`; move every GUI
  branch in `actions/`, `controllers/` and `core/` behind them. Classify all 50
  `PSERV_CONSOLE_BUILD` files; the ones that only guard argparse move to `pservc`.
- Remove the dialog members from `DataController`. Port the environment-variable "Add" to an
  action with parameters.
- `pserv_core.vcxproj`; `pservc` and `pserv5` (ImGui) link it. The ImGui frontend implements
  the host and the before-dispatch dialogs with its existing UI.
- **Exit:**
  - `just build-all` and `just test` pass.
  - `grep PSERV_CONSOLE_BUILD` returns nothing outside `pservc/` and `main_window`/`dialogs`.
  - `pservc` smoke runs every view's list plus one read-only action per view.
  - An ImGui GUI checklist (properties edit, add environment variable, copy, export,
    confirm-delete on a throwaway environment variable) is executed and reported.

### Phase 2 — Off-thread refresh (D5, section 5.1)
- `RefreshRequest`, the shadow container, `RefreshShadow`/`Publish`, a `Clone`/`AssignFrom`
  pair per model (copying must not copy the refcount), the generation and epoch guards, the
  single worker. Fix **F2** in the merge path.
- `pserv_core_tests`:
  - **Merge semantics:** assign, insert, remove; pointer identity preserved for surviving IDs.
  - **Deterministic interleavings**, using a test executor that runs the worker step by step
    and a scripted test controller:
    - (a) an edit is committed between request and publish: the result is discarded, a new
      refresh is requested, and the edited value survives;
    - (b) the target changes mid-refresh: the old result is discarded, the shadow is cleared,
      and no object of the old target appears;
    - (c) the view switches mid-refresh: discarded;
    - (d) a result arrives while a dialog is open: discarded, with one refresh after close;
    - (e) an action completes mid-refresh: discarded;
    - (f) a selection retained for an open dialog survives a publish that removes its objects
      (D4 lifetime).
  - **Stress:** all ten real controllers refreshed on the worker 50× while the main thread
    reads every property of every live object between publishes; ASan in Debug.
- **Exit:** tests pass. The UI-thread cost of `Publish` for Modules (largest view) is measured
  and reported against section 7.

### Phase 3 — pserv6 skeleton **and installer** (D1, D2, D7, D9, D12)
- Prerequisite: MSIS 3.0.9 installed on the build machine (F9).
- `pserv6/pserv6.vcxproj` per Appendix A; WinUI shell with navigation and an empty grid
  placeholder; settings load/save and import (D13); `--selftest` (from Probe 2, command-line
  driven) that loads every view and writes a timing log.
- `just`: build includes pserv6; `package-*` stages `pserv6.exe` + WinAppSDK runtime files +
  `pservc.exe` (excluding `.pdb/.lib/.exp/.winmd`) and symbols separately; MSIS scripts
  `setup/pserv6-{x64,arm64}.msis` and a bundle, using pserv5's UpgradeCodes (D12).
- `just test` additionally extracts the embedded manifest of every built `pserv6.exe` and
  fails unless it says `requireAdministrator` (F6).
- **Exit**, in a disposable ARM64 environment (D12 consequence):
  - `just release` produces both MSIs and the bundle.
  - Installed **over an installed pserv5**: pserv5 is removed, pserv6 launches elevated and
    passes `--selftest`, and `pservc` on PATH is pserv6's.
  - Uninstall leaves no files behind. `msis-check-msi.ps1` is clean.
  - The x64 MSI is built and its extract launches under emulation.

### Phase 4 — Grid (D6)
- The grid control: dynamic columns, sort through the controller, resize, reorder, per-view
  persistence of widths/order/sort (existing `DisplayTable` settings), multi-select, keyboard
  navigation, filter (`MatchesFilter`), `VisualState` colouring, copy selection.
- Investigate the two open probe items with WPR traces: Network Connections' unattributed long
  frames, and the view-switch latency, starting with its worst case (Devices, 569 ms).
- **Exit:** `--selftest`, elevated, ARM64, meets the section 7 budgets on all ten views. A
  keyboard-only checklist is executed.

### Phase 5 — Actions and dialogs
- Context menu, before-dispatch confirmations and parameters (D4), progress/cancel,
  properties dialog with editing, create dialog, export/copy (JSON, text), remote machine
  connect (epoch bump, section 5.1).
- **Exit:** every action of every view is executed once against throwaway targets (a test
  service created for the purpose, a scratch environment variable, a dummy scheduled task),
  with the list reported. Destructive actions are never exercised against real system objects.

### Phase 6 — Parity and polish
- Keyboard shortcuts, zoom, theme switch (D11), About, the user manual `docs/pserv6.md`, and
  `README` updates.
- **Exit:** the pserv5 manual's feature list (`docs/pserv5.md`) is walked item by item against
  pserv6 and each item is marked done, changed (with reason) or dropped (with owner approval).

### Phase 7 — Release and removal
- Remove the ImGui frontend (`main_window.*`, `dialogs/`, `theme/`, `pserv5.vcxproj`), the
  `imgui` submodule, `fkYAML` and `themes/` (D11); update `AGENTS.md`/`CLAUDE.md`.
- **Exit**, in disposable environments, ARM64 executed and x64 under emulation:
  - an upgrade from installed pserv5 5.2 (native arch, and pserv5 x86 on x64) to pserv6;
  - an upgrade from one pserv6 version to the next;
  - an uninstall leaving nothing behind;
  - `just release` reproduces the shipped artifacts.

---

## 7. Performance budgets

These are **targets**, measured by `pserv6 --selftest`, elevated, Release, warm start, on the
ARM64 dev box. Probe 2 shows which ones are plausible; it demonstrates none of them for pserv6
itself. Each becomes pass/fail in Phase 4.

| Metric | Target | Probe 2 (corrected run, not elevated) |
|---|---|---|
| First frame with the default view loaded | ≤ 400 ms | 297 ms (Services refreshed on the UI thread first) |
| View switch: bind → first painted frame, any view ≤ 2,000 rows | ≤ 150 ms | 98–569 ms where no tick intervened (Devices 569 ms); unanalysed |
| Frame interval p95, scrolling + 2 s auto-refresh, any view ≤ 2,000 rows | ≤ 17 ms | 9.0–16.7 ms |
| Frame interval max, same condition | ≤ 50 ms | 22–717 ms (tick-overlapped, except Network Connections' unattributed 187 ms) |
| UI-thread work per refresh tick (`Publish` + diff + realized update), any view ≤ 2,000 rows | ≤ 8 ms | diff + update 0.3–1.5 ms; `Publish` not built |
| Modules (~10k rows): UI-thread work per refresh tick | ≤ 16 ms | not measured (the 6.5 ms bind is collection construction only) |
| Working set after visiting all ten views | ≤ 200 MB | 123–138 MB |
| MSI size per arch | ≤ 20 MB | 14.5 MB (Probe 1) |

---

## 8. Risks

| # | Risk | Mitigation |
|---|---|---|
| R1 | WinUI under elevation. | **Largely retired:** the owner ran the elevated probe through UAC and it worked (2026-11-18). Elevated timings arrive with Phase 3's `--selftest`. Known limitation: drag-and-drop from a non-elevated Explorer is blocked by UIPI; pserv does not use drag-and-drop. |
| R2 | WinAppSDK upgrades change build targets (F3–F5 are all upgrade-sensitive). | Pin the version (D1); upgrade only as an isolated change that re-runs the full Phase 3 exit. |
| R3 | The grid is custom work and the largest single UI item, with two unexplained probe findings. | Scope fixed in Phase 4, which investigates both findings first. Anything beyond pserv5's grid features is out. |
| R4 | **x64 performance is assumed, not measured.** | **Accepted** by the owner (2026-11-18): if it is fine on ARM64 it is assumed fine on x64. x64 is built and functionally checked under emulation every phase. Revisit if a real x64 machine becomes available. |
| R5 | The payload grows from a few MB to ~54 MB on disk / 14.5 MB MSI, and memory to ~120–140 MB. | Accepted cost of WinUI (D1). Language-folder trimming is possible later if size matters. |
| R6 | No automated UI tests. | `--selftest` (load, scroll, refresh, timings) plus `pserv_core_tests`. Interactive checks are listed per phase and reported as checklists, never implied. |
| R7 | Accessibility: rows expose UIA through `ListView`, but cells are plain `TextBlock`s, so a screen reader hears the row name only. | Better than pserv5 (nothing). Per-cell UIA is a post-GA improvement, not a parity item. |
| R8 | Thread-safety of the shared statics used by managers (module cache, service error set). | D5 runs one refresh worker; Phase 2's stress test runs the worker against all ten views. |
| R9 | Any pserv6 MSI removes pserv5 on install (D12). | MSI tests run in disposable environments; side-by-side use before GA runs from the build output. |

---

## 9. What is needed from the owner

1. **A disposable ARM64 test environment** for MSI install/upgrade checks from Phase 3 on: a
   VM or Windows Sandbox, since a pserv6 MSI upgrades pserv5 away (D12). Alternatively, approve
   doing it on the dev box and reinstalling pserv5 afterwards.
2. **MSIS 3.0.9 installed** over the 3.0.6 in Program Files before Phase 3 (F9).

Decisions are settled (section 4). x64 hardware is not required (R4).

---

## Appendix A — WinUI 3 project recipe (as built and run)

Differences from the VS template (`WinUI.Desktop.CppWinRT.SingleProjectPackagedApp`), all
needed for a command-line, unpackaged, self-contained build that MSIS can install:

```xml
<PropertyGroup Label="Globals">
  <AppxPackage>false</AppxPackage>
  <WindowsPackageType>None</WindowsPackageType>              <!-- unpackaged -->
  <WindowsAppSDKSelfContained>true</WindowsAppSDKSelfContained>
  <NuGetTargetMoniker>native,Version=v0.0</NuGetTargetMoniker> <!-- F3 -->
  <RuntimeIdentifiers>win10-arm64;win10-x64</RuntimeIdentifiers> <!-- F3 -->
</PropertyGroup>
<ItemDefinitionGroup>
  <ClCompile><LanguageStandard>stdcpplatest</LanguageStandard></ClCompile>
  <Link>
    <EnableUAC>true</EnableUAC>                               <!-- F6 -->
    <UACExecutionLevel>RequireAdministrator</UACExecutionLevel>
    <AdditionalDependencies>shell32.lib;advapi32.lib;ole32.lib;oleaut32.lib;user32.lib;%(AdditionalDependencies)</AdditionalDependencies> <!-- F7 -->
  </Link>
</ItemDefinitionGroup>
<ItemGroup>                                                    <!-- F5: components, not the meta-package -->
  <PackageReference Include="Microsoft.WindowsAppSDK.WinUI" Version="1.8.260224000" />
  <PackageReference Include="Microsoft.WindowsAppSDK.Foundation" Version="1.8.260222000" />
  <PackageReference Include="Microsoft.Windows.CppWinRT" Version="2.0.250303.1" />
  <PackageReference Include="Microsoft.Windows.SDK.BuildTools" Version="10.0.26100.4654" />
  <PackageReference Include="Microsoft.Windows.ImplementationLibrary" Version="1.0.260126.7" />
</ItemGroup>
<!-- after Microsoft.Cpp.targets; F4 -->
<Target Name="DedupClCompile" BeforeTargets="WarnCompileDuplicatedFilename">
  <RemoveDuplicates Inputs="@(ClCompile)"><Output TaskParameter="Filtered" ItemName="_DedupedClCompile" /></RemoveDuplicates>
  <ItemGroup>
    <ClCompile Remove="@(ClCompile)" />
    <ClCompile Include="@(_DedupedClCompile)" />
  </ItemGroup>
</Target>
```

Other rules:
- Build with `msbuild <proj> -restore`.
- The WinUI PCH defines `NOMINMAX` before `<windows.h>`.
- WinUI translation units and `pserv_core` translation units are separate; neither includes the
  other's PCH.
- Stage the payload without `.pdb/.lib/.exp/.winmd`.
