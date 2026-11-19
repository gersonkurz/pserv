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

## Deferred checks (owner-accepted)

8. **Execute the MSI major upgrade from an installed pserv5 5.2.0 to a 6.x package.** The 6.0.0 version stamp (2026-11-19) keeps the per-arch UpgradeCode `{672E9734-DC0A-4FD9-B25E-8544C621B230}`, and Windows Installer lists the installed 5.2.0 ARM64 product as related to it. So the lineage is as intended, but no upgrade has been executed. The owner declined running it on the dev box ("Do not update the install … focus on the actual app functionality at this point", 2026-11-19). Covered by the plan's Phase 3/7 installer exit criteria at the latest.

9. **Execute the ImGui GUI checklist for the Phase 1 dispatch changes:** properties edit, "Add User Variable" (new parameter modal), copy, "Export to JSON..." (save dialog collected before dispatch), and confirm-delete of a throwaway variable. Phase 1 changed how the ImGui frontend dispatches actions (pre-dispatch confirmation and parameters, `IActionHost`, the properties dialog owning its selection). It is verified to build and link only; the interactive paths are unexecuted. The owner chose to defer this (2026-11-19: "Defer it"), because the ImGui frontend is deleted in Phase 7 and the effort goes to the WinUI app.

## From the pserv6 Phase 1 review (Codex, 2026-11-19, thread 01a1f3ef)

10. **[task] Pre-existing process termination reports success after failure.** [process_actions.cpp:175](C:/Projects/pserv-claude/pserv5/actions/process_actions.cpp:175) counts failed `TerminateProcessById` calls but unconditionally returns `true` at line 182. Consequently, even zero successful terminations produce `AsyncStatus::Completed` and CLI success. Both previous frontend implementations already misreported failures, so correcting this is deferrable from the frontend-separation task. Propagate partial/total failure and verify it with a controlled failure case.

    **Resolved (Phase 1, 2026-11-19):** the terminate work function now returns `success == total`, so any failure ends the operation `Failed` with `Terminated x/y processes` as its error. Verified by `TestDestructiveActionsReportFailure` (PID 0xFFFFFFFE → status Failed); with the old `return true` temporarily restored, the same test fails (status Completed). The same fix was applied to "Uninstall Service", which ignored `DeleteService`'s result.

## Found while executing the Phase 1 CLI matrix (2026-11-19)

11. **Pre-existing: "Open in Explorer" fails for drivers and reports success.** Device (driver) binary paths are stored as `\SystemRoot\System32\drivers\<name>.sys`; the services/devices "Open in Explorer" action passes the folder unexpanded to `ShellExecuteW`, which fails with ERROR_FILE_NOT_FOUND (`service_actions.cpp:570`). The failure is only logged, so `pservc devices open-in-explorer 1394ohci` exits 0. Expand `\SystemRoot` (and `\??\` prefixes), and report a launch failure to the user.

## From the Debug-GUI Direct3D fallback review (Codex, 2026-11-19, thread 01a1f3fe)

12. **[task]** [main_window.cpp:372](/C:/Projects/pserv-claude/pserv5/main_window.cpp:372): Pre-existing initialization defect: `CreateRenderTarget()` logs failures without propagating them, so `InitializeDirectX()` reports success with no render target. `Render()` then returns immediately at line 930, leaving a non-rendering window. Propagate initial render-target creation failure to initialization. Deferrable because this behavior predates the diff and is separate from the missing-debug-layer fallback.
