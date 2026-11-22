# TODO

Findings recorded from reviews, to be addressed outside the change that surfaced them.

## From the pserv6 migration plan review (Codex, 2026-09-18, thread 01a0b39b)

8. **[task] Pre-existing environment-variable rename can lose the original value.**
   [environment_variables_data_controller.cpp:137](pserv5/controllers/environment_variables_data_controller.cpp):
   `CommitPropertyEdits` deletes the original variable before attempting to write the
   renamed variable at line 146. If that write fails, the method returns failure after the
   original has already been removed. Preserve the original until the replacement succeeds
   and define rollback/failure handling, with an executed failure-path check. This predates
   the plan and is deferrable from this documentation change because fixing
   environment-variable mutation semantics is outside its scope.

## From the pserv6 WinUI plan review (Codex, 2026-11-18, thread 01a1ef4f)

5. **[task] Pre-existing service display-name editing uses the wrong column index.** [services_data_controller.cpp:149](/C:/Projects/pserv-claude/pserv5/controllers/services_data_controller.cpp:149) handles display-name edits at index 0, but the constructor declares `Name` at index 0 and editable `Display Name` at index 1. Editing the displayed editable field therefore reaches the default rejection path. Align the edit mapping with column metadata and verify a display-name edit against a disposable service. This predates the plan and is deferrable because fixing existing service-edit behavior is outside this documentation change.

6. **[task] Pre-existing service property commits report success after API failure.** [services_data_controller.cpp:210](/C:/Projects/pserv-claude/pserv5/controllers/services_data_controller.cpp:210) ignores the Boolean result of `ServiceManager::ChangeServiceConfig`, updates the local object, and returns success. The manager returns `false` on failures, including access denial; the GUI can therefore show unapplied settings as successfully saved. Propagate failure, preserve truthful local state, and execute a failure-path check. This predates the plan and is deferrable because repairing service mutation semantics is outside this documentation change.

## From the pserv6 Phase 0 review (Codex, 2026-11-18, thread 01a1ef7c)

7. **[task] Pre-existing CLI parser indexing selects the wrong parser after Services.** [pservc.cpp:66](C:/Projects/pserv-claude/pserv5/pservc/pservc.cpp:66) indexes `subparsers` by controller ordinal, but [data_controller.cpp:124](C:/Projects/pserv-claude/pserv5/core/data_controller.cpp:124) inserts action parsers into that same vector between controller parsers. Consequently, later controllers read options and search for actions on an unrelated parser, explaining the ignored Processes format/filter options and potentially preventing action dispatch. Both implementations are present in HEAD. Associate each controller with its actual parser and verify listing options and read-only action dispatch. This is deferrable because repairing existing CLI dispatch is outside Phase 0.

    **Resolved (Phase 1, 2026-11-19):** `RegisterControllerArguments` returns the controller's own parser, and dispatch uses it (verified by the Phase 1 CLI matrix).

## Deferred checks (owner-accepted)

8. **Execute the MSI major upgrade from an installed pserv5 5.2.0 to a 6.x package.** The 6.0.0 version stamp (2026-11-19) keeps the per-arch UpgradeCode `{672E9734-DC0A-4FD9-B25E-8544C621B230}`, and Windows Installer lists the installed 5.2.0 ARM64 product as related to it. So the lineage is as intended, but no upgrade has been executed. The owner declined running it on the dev box ("Do not update the install … focus on the actual app functionality at this point", 2026-11-19). Covered by the plan's Phase 3/7 installer exit criteria at the latest.

9. **Execute the ImGui GUI checklist for the Phase 1 dispatch changes:** properties edit, "Add User Variable" (new parameter modal), copy, "Export to JSON..." (save dialog collected before dispatch), and confirm-delete of a throwaway variable. Phase 1 changed how the ImGui frontend dispatches actions (pre-dispatch confirmation and parameters, `IActionHost`, the properties dialog owning its selection). It is verified to build and link only; the interactive paths are unexecuted. The owner chose to defer this (2026-11-19: "Defer it"), because the ImGui frontend is deleted in Phase 7 and the effort goes to the WinUI app.

    **Void (2026-11-19):** the ImGui frontend was removed at the owner's request. The same paths are covered by pserv6's Phase 5 action checks.

## From the pserv6 Phase 1 review (Codex, 2026-11-19, thread 01a1f3ef)

10. **[task] Pre-existing process termination reports success after failure.** [process_actions.cpp:175](C:/Projects/pserv-claude/pserv5/actions/process_actions.cpp:175) counts failed `TerminateProcessById` calls but unconditionally returns `true` at line 182. Consequently, even zero successful terminations produce `AsyncStatus::Completed` and CLI success. Both previous frontend implementations already misreported failures, so correcting this is deferrable from the frontend-separation task. Propagate partial/total failure and verify it with a controlled failure case.

    **Resolved (Phase 1, 2026-11-19):** the terminate work function now returns `success == total`, so any failure ends the operation `Failed` with `Terminated x/y processes` as its error. Verified by `TestDestructiveActionsReportFailure` (PID 0xFFFFFFFE → status Failed); with the old `return true` temporarily restored, the same test fails (status Completed). The same fix was applied to "Uninstall Service", which ignored `DeleteService`'s result.

## Found while executing the Phase 1 CLI matrix (2026-11-19)

11. **Pre-existing: "Open in Explorer" fails for drivers and reports success.** Device (driver) binary paths are stored as `\SystemRoot\System32\drivers\<name>.sys`; the services/devices "Open in Explorer" action passes the folder unexpanded to `ShellExecuteW`, which fails with ERROR_FILE_NOT_FOUND (`service_actions.cpp:570`). The failure is only logged, so `pservc devices open-in-explorer 1394ohci` exits 0. Expand `\SystemRoot` (and `\??\` prefixes), and report a launch failure to the user.

    **Resolved (2026-11-19):** `ServiceInfo::GetInstallLocation` resolves `\SystemRoot\...`, `\??\...` and Windows-relative `system32\...` paths, and unquoted paths containing spaces (tested in `TestEditTransactions`); the open actions throw when the launch fails, which both frontends report.

## From the Debug-GUI Direct3D fallback review (Codex, 2026-11-19, thread 01a1f3fe)

12. **[task]** [main_window.cpp:372](/C:/Projects/pserv-claude/pserv5/main_window.cpp:372): Pre-existing initialization defect: `CreateRenderTarget()` logs failures without propagating them, so `InitializeDirectX()` reports success with no render target. `Render()` then returns immediately at line 930, leaving a non-rendering window. Propagate initial render-target creation failure to initialization. Deferrable because this behavior predates the diff and is separate from the missing-debug-layer fallback.

    **Void (2026-11-19):** `main_window.cpp` was removed with the ImGui frontend.

## From the pserv6 Phase 2 review (Codex, 2026-11-19, thread 01a1f410)

13. **[task] Pre-existing: service edits report success after configuration failure.** [services_data_controller.cpp:187](/C:/Projects/pserv-claude/pserv5/controllers/services_data_controller.cpp:187) ignores the Boolean result of `ServiceManager::ChangeServiceConfig`, updates the live object, and returns success even when the API wrapper reports failure. Users can be shown settings that were never applied. Check the result and preserve failure reporting. Deferrable because this unchanged edit-transaction defect predates the off-thread refresh task. *(Same defect as item 6.)*

    **Resolved (2026-11-19, items 6 and 13):** `CommitPropertyEdits` returns false and leaves the object unchanged when `ChangeServiceConfig` fails. The "Set Startup" actions also report failures now (they ignored the result and always said "Automatic").

14. **[task] Pre-existing: environment-variable rename can lose the original value.** [environment_variables_data_controller.cpp:115](/C:/Projects/pserv-claude/pserv5/controllers/environment_variables_data_controller.cpp:115) deletes the original variable before creating the replacement. If the subsequent write fails, it returns failure without restoring the deleted value. Make rename failure preserve the original data and verify with a throwaway variable and injected write failure. Deferrable because this unchanged mutation path is outside Phase 2. *(Same defect as item 8 in the first section.)*

    **Resolved (2026-11-19, items 8 and 14):** a rename writes the new variable first and deletes the old one afterwards (undoing the new one if that fails), refuses to overwrite an existing variable, and treats a change of case only as an update in place. Tested on throwaway user variables in `TestEditTransactions`.

## Open after Phase 4 part 1 and the pserv5 removal (2026-11-19)

15. **Mouse probe of the grid header is flaky (unexplained).** Across the Release runs of the header probe (click sorts, drag reorders, gripper resizes), roughly 1 run in 5 lost the click or ended the resize early through a lost pointer capture. It could not be reproduced with header debug logging in place (4/4). It may be injected-input loss on the dev VM or a real lost release. Keyboard sorting (Alt+S) is unaffected. The header logs press, release and capture loss at debug level for a future report.

    **Update (2026-11-19):** after the off-screen cache change removed the long frames following a view switch (item 16), the same probe passed 6 of 6 runs. That fits lost input during those frames, but 6 runs do not prove it; the item stays open as an observation.

16. **Phase 4 exit not yet met.** The section 7 budgets: frame max while scrolling is over 50 ms, and the first-visit switch is over 150 ms for slow enumerations. Still to do: the WPR investigations of long frames and view-switch latency, an elevated self-test run (the owner's), and the full keyboard-only checklist.

    **Update (2026-11-19):** the long frames came from the ListView's off-screen cache (four viewports by default) being built in the frames after a view switch; with `CacheLength=1` two consecutive unelevated Release runs on ARM64 show no frame over 50 ms (max 30 ms, p95 about 9 ms; the self-test now lists every frame over 50 ms with its time). The self-test also measures switching back to a loaded view: 24-111 ms (budget 150 ms); a first visit additionally contains the first enumeration (Services about 150 ms, Scheduled Tasks about 300 ms, on the worker). A WPR trace was not needed. Still open: the elevated run (the owner's) and the keyboard-only checklist.

17. **GUI manual and screenshot.** `docs/pserv6.md` is to be written (Phase 6). `docs/main.png`, still referenced by the README, shows pserv5.

    **Resolved (Phase 6, 2026-11-19):** `docs/pserv6.md` and `docs/pserv6.png` exist; the README links them; `docs/main.png` is removed.

18. **Installer exit criteria of Phase 3/7 still need a disposable environment:** install over pserv5, the pserv6-to-pserv6 upgrade, uninstall leaving nothing behind. On the dev box, MSIs are checked only by `just check-msi` (administrative extract plus a self-test of the extracted payload).

## From the pserv6 Phase 5 review (Codex, 2026-11-19, thread 01a1f61a)

19. **[task] Pre-existing service mutations ignore the remote target.**  
   [service_manager.cpp:224](C:/Projects/pserv-claude/pserv6/windows_api/service_manager.cpp:224) opens the local SCM with `nullptr`; Stop and other mutation helpers do likewise. [Service property commits](C:/Projects/pserv-claude/pserv6/controllers/services_data_controller.cpp:187) also omit the target. Actions on remotely enumerated rows therefore modify the same-named **local** service; Delete Registry Key likewise uses local HKLM. Carry the target through mutation APIs and dispatch, or prevent unsupported remote mutations. This predates the diff and is deferred under the requested scope rule because repairing shared service targeting expands beyond frontend wiring.

    **Resolved (2026-11-19):** every `ServiceManager` mutation takes the machine name; the service actions and the properties commit pass `DataController::GetPublishedTarget()`, the machine the rows were enumerated from. Actions that need this computer declare `DataAction::RequiresLocalMachine()` (Open in Registry Editor/Explorer/Terminal, Delete Registry Key): they throw for remote objects and the GUI does not offer them. Executed: `--selftest-actions` enumerates Services through the remote path (this computer by name) and checks that an action is dispatched for the rows' machine, that a local-only action is refused, and that the controller still counts as remote between Connect to Local Machine and the next publish. **Not executed:** an actual Start/Stop/edit against a second machine (none available; needs administrator there) - part of the manual checklist, item 22.

20. **[task] Pre-existing service Display Name editing uses the wrong property index.**  
   [services_data_controller.cpp:126](C:/Projects/pserv-claude/pserv6/controllers/services_data_controller.cpp:126) handles Display Name as column `0`, while its metadata places it at `1`. The new properties dialog correctly passes `1`, so changing Display Name is rejected and prevents that object’s transaction from committing. Use the property enum and add a transaction check. This is deferrable because the incorrect controller mapping already exists in HEAD.

    **Resolved (2026-11-19):** the edit transaction switches on `ServiceProperty`; `TestEditTransactions` checks every editable column is accepted at its own index and Name/Status are refused.

## Phase 5 checks deferred with the owner's acceptance (2026-11-19: "Accept. I will do a manual review anyway.")

21. **Service actions against a throwaway service, elevated.** Start, Stop, Restart, Pause and Resume need a service program that actually runs (and accepts pause), e.g. a tiny test service built for the purpose. Uninstall Service and Delete Registry Key need such a service too. `pserv6 --selftest-actions`, elevated, covers only Start (expected failure with cmd.exe) and Uninstall Service today; Close Connection needs a connection of its own.

22. **Interactive checklist for the Phase 5 dialogs** (the owner's manual review): the context menu (right-click, Shift+F10, separators, "(N selected)", destructive colour), the confirmation dialog for each destructive action, the parameter dialog ("Add User/System Variable"), the save dialog (Export to JSON/Plain Text...), the progress dialog with Cancel (e.g. Stop on a slow service), the properties dialog (each editor type, multiple objects, an action button, OK and Cancel), Delete and Enter keys, double-click, and remote connect / connect local (with a reachable second machine). Automated so far: the dispatch of 32 actions without dialogs, and a keyboard probe of the Delete confirmation (default No, then Yes).

## Open at the end of the 2026-11-19 session (owner boarding; start here next time)

23. **The Debug build of pserv6 crashes a few seconds after start (`just run`), exit code 0xC0000409.** Reproduced unelevated, so it is not the elevation prompt. Under cdbARM64 it shows as heap corruption (`c0000374`) detected in `ntdll!LdrpFreeTls` when a thread exits, preceded by `Microsoft.UI.Xaml.dll: 8001010E RPC_E_WRONG_THREAD`. The Release build does not show it (all self-tests since Phase 5 ran Release; the last Debug GUI runs that worked were the Phase 4 header probes at `8cb1646`). Not found yet: an ASan Debug build of the GUI exited with code 1 once without a readable report and ran 90 s clean the second time. Next steps: bisect Debug builds between `8cb1646` and HEAD (suspects: Phase 5 window subclass / ActionRunner timers, Phase 6 theme code, `spdlog::flush_every` from the log-flush change in `8cb1646` itself); get the ASan report (run from a console so stderr is visible). Until fixed, run Release: `just build-release`, then `pserv6\bin\ARM64\Release\pserv6.exe` from an elevated shell. A heap overrun that only Debug detects may exist in Release too.

    **Update (2026-11-22, x64 PC):** the x64 Debug build ran 45 s unelevated without crashing (idle, one run). So far the crash is seen only on ARM64 (Parallels VM); the bisect has to happen there.

24. **Owner's list of what is missing for a feature-complete release (2026-11-19, verbatim: "Base16 themes. Font selection. Ctrl+Mousewheel. Context menus for multiple entries. Icons for action buttons. Properties boxes handling multi-selected items. auto-refresh performance."):**
    - *Base16 themes:* removed by plan decision D11 (Fluent Light/Dark only). The owner now lists them as missing: D11 needs to be revisited (the YAML themes are in git history at `8cb1646`, `pserv5/themes/`).
    - *Font selection:* not implemented (pserv6 has table zoom only).
    - *Ctrl+Mousewheel:* implemented as table zoom (50-200 %) in `3dafef7`, executed only via the settings/keyboard path; the wheel itself is untested, and it does not scale the whole UI as pserv5's did.
    - *Context menus for multiple entries:* implemented (actions apply to the whole selection, "(N selected)"), never checked by hand.
    - *Icons for action buttons:* not implemented (menus and buttons are text only).
    - *Properties with multiple selected items:* implemented as one dialog with an item picker (pserv5 had tabs), never checked by hand.
    - *Auto-refresh performance:* measured unelevated only (publish at most 4 ms on the UI thread, no frame over 50 ms); the owner's impression on an elevated run is the reference. Needs concrete observations.
    The owner has since run the Release build (2026-11-19): "overall pretty good", with the narrow table centred instead of left-aligned (fixed in `6a7a59d`) and "known gaps remain - themes, fonts".

## Things to do next (2026-11-19, after comparing with the Codex and Gemini attempts)

The owner's verdict: Gemini's version (`pserv6-gemini`) is the best so far in UI speed and design; Codex's is `codex-branch`. Both were surveyed read-only. Nothing below is started. Suggested order:

25. **Base16 themes** (both have them; reverses plan D11 - update the plan). Restore the 20 YAML files from `8cb1646:pserv5/themes/` into `pserv6/gui/themes/`, copy them next to `pserv6.exe` (and into the MSI payload and `check-msi`). Parser: the files are flat `name` + `palette.base00..base0F`; a small hand-written reader avoids bringing the fkYAML submodule back (both others use fkYAML, GUI only). Apply as both do: insert brushes for about 34 WinUI resource keys into `Application.Resources` (base00 page/solid background, base01 layer/card/control/menu/text-control fill, base02 strokes/selected/hover, base05/04/03 text primary/secondary/tertiary-disabled, base0D accent and selected navigation item, dialog background base00), then bounce `Root.RequestedTheme` so controls re-resolve; dark or light from the luminance of base00; high contrast forces System. Grid rows: normal base05, highlighted base0D, disabled base03, destructive base08. Menu: Themes > System / Light / Dark / Dark themes > / Light themes >, check mark on the active one. `application.theme` holds the name; the pserv5 import can then keep the theme instead of resetting it.
    **Done (2026-11-22, x64):** as described, with the theme's `variant` deciding dark/light, about 70 resource keys (`THEME_KEYS` in `MainWindow.xaml.cpp`), an opaque base00 page instead of Mica and the title bar in base00/base05. The pserv5 import keeps the theme (executed: the first start here came up in the imported "Tomorrow Night Eighties"). Executed: the self-test applies all 20 themes and checks grid text, accent, destructive colour and dark/light, then the Windows setting again; screenshots of Tomorrow Night Eighties, Dracula, Solarized Light and back to the Windows setting, picked through the View menu by UI Automation. **Not looked at:** menus, context menu, dialogs and the properties dialog under a theme (flyouts are separate windows the capture did not include), high contrast, the MSI payload (`themes\` is copied by the build and staged by robocopy `/E`; `just check-msi` not run since).

26. **Font selection** (both use Win32 `ChooseFontW`; points to DIPs x96/72). Table and header font family in `application.fontFamily`; the existing zoom stays the size control. Decide whether chrome follows (Codex walks the visual tree, Gemini changes the table only - the latter is simpler and what the owner praised).
    **Done (2026-11-22, x64):** View > Table font > Choose... (`ChooseFontW`, family only) / Default font; `application.FontFamily`; table and header only, chrome keeps the Windows font. Executed: start with `FontFamily = 'Consolas'` in a temporary config, screenshots at 50 % and 200 % zoom, alone and with a theme. **Not executed:** the font dialog itself (a modal Win32 dialog; no agent has clicked through it).

27. **Icons.** Segoe Fluent glyph per view in the navigation pane; glyph per action in the context menu, the Properties dialog buttons and a selection-following toolbar (icon-only buttons with tooltips, destructive ones red, rebuilt only when the action set changes); icons on the File/View/Help items. Gemini's action-to-glyph table (`MainWindow.xaml.cpp:63-92` there) is the more complete one; give "Set Startup: ..." actions a real glyph instead of its fallback.
    **Partly done (2026-11-22, x64):** glyph per view in the navigation pane (screenshot checked) and per action in the context menu (`gui/icons.h`; an action without an entry gets no icon). **Not looked at:** the context menu itself (a flyout window the capture does not include) - the action glyph codes are from memory of the Segoe Fluent Icons table and need the owner's eye. **Still to do:** icons on the Properties dialog buttons and the File/View/Help items, and the selection-following toolbar.

    **Done (2026-11-22, later):** icons on the Properties dialog buttons and on File/Help items; the toolbar shows icon buttons for the selected rows' actions (those with a glyph of their own, without Copy.../Set Startup...; rebuilt only when the set changes). Screenshot checked with a selected service: Stop, Restart, Registry Editor, Explorer, Terminal, Uninstall, Delete Registry Key render as intended. Not seen rendered: the glyphs of the other views' actions, the dialog buttons, the menu icons.

28. **Ctrl+mouse wheel anywhere in the window**, not only over the table (handler on `Root` with `handledEventsToo`). The table must stay left-aligned at every zoom (owner's hint) - check header and rows at 50 % and 200 % by screenshot.
    **Done (2026-11-22, x64):** the handler sits on the window's root and steps once per full wheel notch (it stepped once per event before, which races through the range on a high-resolution wheel or touchpad). Screenshots at 50 % and 200 %: header and rows left-aligned. **Not executed:** a real Ctrl+wheel input. **Unexplained, seen once:** one pair of runs started with 200 % and 50 % in the config showed 100 % and 90 %; three repeats with the same config were correct. Possibly real Ctrl+wheel input on the owner's desktop while the window was under the pointer (it was the per-event handler then). Zoom changes are now logged at debug level ("Zoom: ...") to tell next time.

29. **Status bar**: visible / total / selected / highlighted / disabled counts, auto-refresh state ("every 2 s", "off", "manual view"), time and duration of the last refresh. Today there is one text line.
    **Done (2026-11-22):** left: shown (of total) | selected | highlighted | disabled; right: auto-refresh state | time and duration of the last refresh. Screenshot checked.

30. **Table polish from Gemini**: alternating row background, right-aligned numeric columns, sort chevron glyph in the header, always-visible non-overlay scrollbars, `ShowsScrollingPlaceholders="False"`, empty `ItemContainerTransitions`, a fixed header that follows the list's horizontal offset (pserv6 scrolls header and list together inside one horizontal `ScrollViewer`, which gives the list an unbounded width - verify that row virtualization still measures only visible rows), live column resize by writing the realized rows' column widths.
    **Done in part (2026-11-22):** alternating row background (its own theme key), `ShowsScrollingPlaceholders="False"`, empty `ItemContainerTransitions`, the view title above the toolbar removed (it cost 100 px). Already there before: right-aligned numeric columns, a sort marker, live column resize. **Not done, on purpose:** always-visible scrollbars (needs a re-templated ScrollViewer; Windows has its own setting for it) and the fixed header in a separate scroller (the present single horizontal ScrollViewer works and virtualizes: the self-test's container counts stay at the visible range).

31. **Columns dialog**: show/hide and reorder columns, reset to defaults (both have it; pserv6 has drag-reorder and resize only, no hiding).
    **Done as menus (2026-11-22):** right-click on the header, or View > Columns: a check mark per column and "Reset columns"; reordering stays drag and drop. `ColumnHidden` per view (column indices). The self-test hides and shows a column. No separate dialog - say so if one is wanted.

32. **Context menu for several rows**: both others offer the intersection of the actions available for every selected row; pserv6's rule (`ShowContextMenu`) should be checked against that and made the same. Properties for several rows: pserv6 has an item picker, Gemini a `Pivot` tab per item (as pserv5 had) - owner, 2026-11-19: "item picker is fine". Keep the picker.
    **Done (2026-11-22):** the context menu offers an action only if every selected row allows it (it asked the clicked row only, and then ran the action on all of them). Compiled and self-tested; not exercised by hand.

33. **Auto-refresh performance**: Gemini's refresh keeps containers and re-renders only realized rows, writing a cell only when its text or brush changed, and leaves the selection untouched unless the index set changed. pserv6 does incremental remove/insert up to 64 changes, else `ReplaceAll`. Measure both elevated on the same machine (Processes at 1 s) before changing anything; the owner's impression is the reference.
    **Changed without an elevated measurement (2026-11-22):** a refresh that changes the row order (a view sorted by a value that changes, e.g. Processes by memory) reset the ListView every time: all containers rebuilt, keyboard focus lost. Such a refresh now rewrites the existing rows (`RowItem::Set`); the selection follows its objects. Self-test: a re-sort causes no reset, selection and scroll position kept; publish cost on the UI thread at most 2.6 ms here. The elevated comparison with Gemini's build is still the owner's.

34. **Design**: custom title bar (`ExtendsContentIntoTitleBar`) with app icon, version and a Local/Remote badge; filter placeholder "Filter <view>... (Ctrl+F)"; inline progress card with Cancel instead of a modal progress dialog for long actions.
    **Done (2026-11-22):** the filter placeholder ("Filter <view> (Ctrl+F)"). **Not done:** the custom title bar (drag regions around a live MenuBar cannot be checked without real pointer input) and the inline progress card (the modal progress dialog works and is covered by the action self-test).

35. Then the open items above: 23 (Debug crash), 16, 18, 21, 22. Update `docs/pserv6.md` and the plan for whatever of 25-34 lands.

Build note for the x64 machine: `git submodule update --init --recursive`, then `just build-all && just test`, `just selftest -RunAsInvoker`. All measurements so far are from an ARM64 Parallels VM; x64 numbers do not exist yet.

**x64 baseline (2026-11-22, owner's main PC, unelevated Release):** `just build-all`, `just test` (30436 checks, 0 failures) and `just selftest -RunAsInvoker` pass. Frames: p95 about 17 ms, max 17-22 ms in every view (vsync-bound); switching back to a loaded view 19-45 ms; first visits over the 150 ms budget are the enumerations themselves (Services 193 ms, Devices 203 ms, Scheduled Tasks 371 ms; Modules 1.4 s for 12,330 rows).

## Found on the x64 PC (2026-11-22)

36. **Pre-existing: pserv6 could hang on exit while the Windows view was refreshing.** The refresh worker read window titles with `GetWindowTextLengthW`/`GetWindowTextW`, which send a message to windows of the own process; on close the UI thread joins the worker and never answers. Seen as `just selftest` not returning (3 of 4 runs after the icon change shifted the timing; stacks taken with cdbX64: UI thread in `RefreshWorker::~RefreshWorker` -> `thread::join`, worker in `WindowManager::GetWindowTextUtf8` -> `SendMessageWorker`).

    **Resolved (2026-11-22):** `GetWindowTextUtf8` uses `InternalGetWindowText`, which sends nothing. 4 of 4 self-test runs exit with code 0 afterwards; `pservc windows` still lists titles. The same blocking would have delayed every Windows refresh while the UI thread was busy. Note that `scripts\selftest.ps1` waits 10 minutes before it reports a hang.

37. **State at the end of 2026-11-22 (x64 PC), for the owner's feedback.** Executed, all unelevated: `just build-all`, `just test` (30436 checks), `just test-asan` (30887 checks, 50-round stress), the grid self-test and `--selftest-actions` (32 actions) on x64 Release, `just release` (both MSIs and the bundle), `just check-msi x64` and `arm64` (232 files each, no difference), and the self-test of the payload extracted from the x64 MSI (themes found). `scripts\check-msi.ps1` no longer needs `Get-FileHash` (not found when Windows PowerShell is started from PowerShell 7). The x64 Debug GUI ran 40 s without the crash of item 23.

    Still open, in the order suggested: the owner's look at menus, dialogs and action glyphs under a theme (item 25/27 notes); a new `docs/pserv6.png` (the present one predates themes, icons and the status bar); item 23 on ARM64; the elevated runs (16, 21, 33); the installer criteria in a disposable environment (18); the interactive checklist (22); a custom title bar and an inline progress card if wanted (34).

38. **Owner's feedback of 2026-11-22 on the build of item 37** (checks 1-5 of that round passed: themed context menu and dialogs, in-place refresh under keyboard navigation elevated, font dialog, Ctrl+wheel, closing during a Windows refresh). Verbatim: "Horizontal scroll bar works fine, vertical scroll bar is missing. Action buttons (you already have the icons, and the text should come out as screenshots). I think 'Windows controller' does not support auto-refresh, but one would not know from the UI. When starting/shutting down/doing anything with a service, I'd expect a progress bar [...]. Properties dialog: actions should be buttons with icons only, not 'icons + text': the text should be tooltip for the buttons. Otherwise, not all actions fit the screen."

    **Done (2026-11-22):**
    - *Vertical scrollbar:* the list scrolls in both directions itself (its scrollbars are at the edges of the view); the header sits in a bar-less ScrollViewer that DataGrid keeps at the list's horizontal offset (ViewChanged). Executed: screenshot with both scrollbars; scrolled fully right through UI Automation, header and rows aligned; self-test passes (container counts unchanged, so rows still virtualize).
    - *Auto-refresh not supported:* in Windows, Modules and Scheduled Tasks the Auto-refresh switch is greyed out with a tooltip, and the status bar reads "No auto-refresh in this view (F5 refreshes)".
    - *Progress:* a bar with the operation's text and Cancel above the status bar replaces the modal progress dialog (ActionRunner::Callbacks::progress, MainWindow::ShowActionProgress); indeterminate while the operation reports no fraction; it stays about a second after the end so a fast start is still seen. **Not executed:** a real service start/stop (needs elevation) - the owner's check.
    - *Properties dialog:* icon-only action buttons, the name as tooltip; an action without a glyph gets "...". The three Set Startup actions have glyphs of their own now. **Not seen rendered.**
    - Also: "Processes" instead of "Processs" in the status bar.

    **Open question to the owner:** "Action buttons (you already have the icons, and the text should come out as screenshots)" - read as "icon buttons with the name as tooltip", which is what the toolbar does; if it meant something else (all actions in the toolbar, including Copy... and Set Startup...? text labels?), it is not done.

39. **Owner's second feedback of 2026-11-22** (the round of item 38 accepted: "Everything else fine"). Done:
    - *Progress names the service:* the service manager's state messages ("Service state: Start Pending") replaced the action's "Starting service 'x'... (1/2)" within 100 ms; every progress message of Start/Stop/Restart/Pause/Resume now carries the name and position ('x' (1/2): Service state: ...). **Not executed** (needs elevation).
    - *Double-click on a column edge* fits the column to its header and content (DataGrid::AutoSizeColumn; measures the 30 longest texts, not every row). The self-test calls it and checks a sane width; the double-click itself is the owner's check.
    - *Toolbar in the menu bar's row:* refresh, sort, auto-refresh, filter and the action buttons sit right of File/View/Help; the table gained that row. Screenshot checked. Not done: putting the row into the title bar (ExtendsContentIntoTitleBar) - possible, needs pointer testing of the drag regions.
    - *Found on the way:* the sort marker in the header was mojibake on this PC (raw UTF-8 triangles in a source without BOM, compiled with the system code page 1252). Now ▲/▼ escapes; no other project source has non-ASCII bytes.

## Notes for the next session (written 2026-11-19 on the ARM64 machine; that session's memory and transcript do not travel)

Where things stand, in the owner's words: Gemini's version is "best bang-for-the-buck" and was finished first by far, but "not perfect"; Codex's finds more bugs but is a less polished experience; this branch is "not there yet". Each has pros and cons - take ideas from both, do not copy either wholesale. The other two trees are other agents' work: read-only (`git fetch`, then `git show origin/pserv6-gemini:<path>` / `origin/codex-branch:<path>`; their GUI sources are `pserv6/MainWindow.xaml.cpp` and `pserv6/ui/` respectively).

Standing decisions of the owner:
- Branch `pserv6-claude`; commit per step and push only when asked. `main` stays untouched.
- Never install an MSI on a development machine (it upgrades the installed pserv5 away). Check MSIs with `just check-msi <arch>` only.
- The yaaadabi review loop is off (too slow). Verify yourself: `just build-all && just test`, `just selftest -RunAsInvoker`, `scripts\selftest.ps1 -Actions`, and a screenshot for anything visual.
- Phase 5's coverage gaps are accepted ("I will do a manual review anyway"); no x86; settings in `%APPDATA%\pserv6`.
- `open-terminals.cmd` is the owner's local file: keep it out of commits.
- Do not call the build feature-complete. It is not, and the owner said so.

Things that cost time on the first machine:
- Avoid UAC prompts when launching pserv6 from a script: set `__COMPAT_LAYER=RunAsInvoker` (the manifest asks for administrator). Anything that needs elevation (service Start/Stop, items 16 and 21) has never been run by an agent - say so instead of implying it.
- The Debug GUI crashes after a few seconds (item 23, seen on ARM64 only so far - check whether x64 does too, that alone narrows it). Use Release for running and for every input or timing probe; Debug's UI thread is seconds slow anyway.
- Probe artefacts that looked like app bugs: a UI Automation client in the same process stalls the app (run UIA from a child process); a clipboard owner that does not answer blocks Ctrl+C; keys sent while pserv6 is not the foreground window go elsewhere (check the foreground window first, click the title bar to activate).
- Files copied with `Copy-Item` keep their old timestamps, and MSBuild then links stale objects (LNK2019). Touch copied sources.
- Scripts that edit sources: backslashes in non-raw Python strings or heredocs silently became control characters several times (`\b`, `\v`, `\p`). Use the editor tool, or raw strings, and scan for control characters afterwards.
- `KeyboardAccelerator` on the `ListView` does not fire; use `KeyDown`. A fixed-width element without `HorizontalAlignment="Left"` is centred. After a runtime theme change Mica must be re-attached. One `ContentDialog` at a time (`ActionRunner::ReserveDialog`).
- A PCH out-of-memory error (C3859/C1076) on `just build-all` was memory pressure; a retry built.
