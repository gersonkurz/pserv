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
