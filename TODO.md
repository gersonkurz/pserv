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
