# pservc Console Implementation Plan

## Overview
Implement a full-featured console interface for pserv5 that exposes all controller functionality via command-line arguments.

## Architecture
- **Command Structure**: Controller as subcommand (e.g., `pservc services list`, `pservc processes terminate 1234`)
- **Output Format**: Custom console.h/cpp with ANSI escape sequences (std::format compatible)
- **Actions**: Map DataAction objects to subcommands with arguments

## Completed Tasks

### ✅ Phase 0: Console Infrastructure Setup
**Completed**: 2025-11-23
- ✅ Added console.h/cpp with ANSI color support and UTF-8 handling
- ✅ Created BaseApp for unified logging/config initialization
- ✅ Fixed spdlog to use debug+file only (no console output)
- ✅ Fixed config logger static initialization issue (section.cpp)
- ✅ Verified std::format works with custom console system - NO need for fmt switch

**Note**: Phase 1 (fmt::format switch) is OBSOLETE - std::format works fine with our console system.

### ✅ Phase 3: Controller Subcommand Registration (COMPLETED)
**Completed**: 2025-11-23

- ✅ **Step 3.1**: Design subcommand structure
  - Each controller registers as subcommand (e.g., "services", "processes")
  - Common arguments: `--format`, `--filter`, `--sort`
  - Controller names converted to lowercase-hyphenated (e.g., "network-connections")

- ✅ **Step 3.2**: Implement base RegisterArguments() pattern
  - Made `DataController::RegisterArguments()` virtual
  - Added common arguments (format, filter, sort)
  - Base implementation in data_controller.cpp

- ✅ **Step 3.3**: Fixed argparse lifetime issues
  - argparse stores reference_wrapper, needs persistent storage
  - Created vector<unique_ptr<ArgumentParser>> in main()
  - Pass subparsers vector to RegisterArguments() for storage

- ✅ **Step 3.4**: Fixed memory leaks on --help
  - argparse calls std::exit(0) on --help by default
  - Set exit_on_default_arguments=false for main parser and all subcommands
  - Allows proper destructor cleanup

- ✅ **Step 3.5**: Verify and commit
  - ✅ `pservc --help` shows all controller subcommands
  - ✅ `pservc services --help` shows common options
  - ✅ No memory leaks on --help
  - ✅ Committed: "Implement controller subcommand registration for pservc"

**Files Modified**:
- `pservc/pservc.cpp`: Added subparsers storage, disabled exit on help
- `core/data_controller.h`: Added RegisterArguments() with subparsers parameter
- `core/data_controller.cpp`: Implemented base RegisterArguments()

### ✅ Phase 2: Console Output Infrastructure (COMPLETED)
**Completed**: 2025-11-24

- ✅ **Step 2.1**: Create console table renderer
  - Created `console_table.h/cpp`: Table rendering for DataObject collections
  - Uses console.h color macros for formatting
  - Handles column widths, alignment based on DataObjectColumn metadata
  - Automatic width calculation with sampling (first 100 rows)
  - Width limits (min 3, max 50 chars per column)

- ✅ **Step 2.2**: Implement DataObject console rendering
  - Methods to render DataObject as table row
  - Methods to render DataObjectContainer as formatted table
  - Uses GetVisualState() for row coloring (highlighted=green, disabled=gray, normal=default)
  - Support for multiple output formats (table, json, csv)

- ✅ **Step 2.3**: Add to Visual Studio project
  - Added console_table.cpp and console_table.h to pservc.vcxproj
  - Ready to build

**Files Created**:
- `pservc/console_table.h`: ConsoleTable class definition with OutputFormat enum
- `pservc/console_table.cpp`: Full implementation with table/json/csv rendering

### ✅ Phase 5: Command Execution and Output (COMPLETED)
**Completed**: 2025-11-24

- ✅ **Step 5.1**: Implement basic command dispatch in main()
  - Parse arguments and identify which subcommand was used
  - Get corresponding controller from DataControllerLibrary
  - Call controller->Refresh() to load data
  - Fixed format argument parsing from subcommand (not main program)

- ✅ **Step 5.2**: Implement list functionality (default action)
  - Render all objects from controller using console table renderer
  - Format based on --format argument (table/json/csv) - all working
  - Default sort by first column ascending for predictable output
  - Fixed UTF-8 character width calculation (German umlauts, emojis)
  - Handle UTF-16 surrogate pairs for extended Unicode
  - Strip newlines from cell content to prevent table breakage
  - Case-insensitive, locale-aware sorting with CompareStringEx

- ✅ **Step 5.4**: Error handling and user feedback
  - Handle invalid arguments gracefully
  - Colored error messages (red) using console.h
  - Success messages (green) using console.h

**Files Modified**:
- `pservc/pservc.cpp`: Command dispatch, format parsing, default sorting
- `pservc/console_table.cpp`: UTF-8 width calculation, newline stripping, rendering
- `core/data_object_container.cpp`: Case-insensitive Unicode-aware sorting

**Note**: Steps 5.3 (filter/sort arguments) and action commands moved to Phase 4

## Remaining Tasks

### Phase 4: Action Command Integration
**Goal**: Map DataAction objects to executable console commands
**Status**: ✅ COMPLETED (2025-11-24)

**Architecture Summary**:
- Actions are verbs after controller name: `pservc services start <name>`
- Target selection by first column (Name), exact match
- Destructive actions require `--force` flag
- Actions can register custom arguments via new `RegisterArguments()` method
- DataAction receives parsed arguments for custom parameter handling

**Key Design Decisions**:
1. **Command Structure**: Actions as verbs (e.g., `pservc services start BITS`)
2. **Target Selection**: First column exact match (e.g., service Name)
3. **Multi-target**: Space-separated names (e.g., `pservc services start BITS Spooler`)
4. **Confirmations**: Destructive actions check `IsDestructive()` and require `--force`
5. **Custom Arguments**: Actions like export add `--output <file>` via `RegisterArguments()`

- [x] **Step 4.1**: Extend DataAction base class
  - Add virtual `RegisterArguments(argparse::ArgumentParser &cmd)` method (default: no-op)
  - Keep existing `Execute(DataActionDispatchContext &ctx)` signature
  - Actions access custom arguments via ctx (need to extend context)
  - Examples:
    - ExportAction adds `--output <file>` argument
    - Destructive actions check for `--force` flag in Execute()

- [x] **Step 4.2**: Extend DataActionDispatchContext
  - Add `argparse::ArgumentParser *m_pActionParser` for action-specific arguments
  - Console variant doesn't use HWND, m_pAsyncOp (set nullptr)
  - Keep m_selectedObjects, m_pController as-is
  - Actions query parsed arguments via m_pActionParser->get<T>(arg)

- [x] **Step 4.3**: Update controller RegisterArguments() to register actions
  - After registering common arguments (--filter, --sort, --col-*):
  - Get all actions from GetActions() (need unified method, not state-dependent)
  - For each action:
    - Create action subcommand: `cmd_name + "_" + action_name` (e.g., "services_start")
    - Add positional argument for target name(s): `.remaining()` or `.nargs(argparse::nargs_pattern::at_least_one)`
    - Add `--force` flag for destructive actions (`action->IsDestructive()`)
    - Call `action->RegisterArguments(action_cmd)` for custom arguments
    - Store action subparser in subparsers vector

- [x] **Step 4.4**: Create GetAllActions() helper method
  - Current: Actions created based on object state (e.g., CreateServiceActions(state, controls))
  - Problem: Need all possible actions at registration time, not per-object
  - Solution: Add `GetAllActions()` to DataController (virtual, controller-specific)
  - Returns vector of all possible actions (ignoring availability)
  - Service example: return {start, stop, restart, pause, resume, set-startup-*, export-*, delete, ...}

- [x] **Step 4.5**: Implement action dispatch in pservc.cpp main()
  - After checking for list-only mode (no action subcommand used):
  - Loop through all actions to find which action subcommand was used
  - If action found:
    - Parse target name(s) from positional arguments
    - Find matching DataObject(s) by first column exact match (case-insensitive)
    - If destructive and no `--force`: print error and exit
    - Create console DataActionDispatchContext with:
      - m_selectedObjects = matched objects
      - m_pController = controller
      - m_pActionParser = action subparser
      - m_hWnd = nullptr, m_pAsyncOp = nullptr (console doesn't use GUI)
    - Call action->Execute(ctx)
    - If ctx.m_bNeedsRefresh: call controller->Refresh()
    - Report success/failure with colored console output

- [x] **Step 4.6**: Handle async operations in console
  - Console doesn't have progress dialog (m_bShowProgressDialog ignored)
  - AsyncOperation still runs, but output to console instead
  - Option 1: Print progress updates to console (ReportProgress messages)
  - Option 2: Simple "Working..." message, then final result
  - For now: No progress output, just wait for completion and report result

- [x] **Step 4.7**: Implement ExportAction for console
  - Export actions need `--output <file>` argument (no file dialog in console)
  - In RegisterArguments(): add `--output` required argument
  - In Execute(): read filename from ctx.m_pActionParser->get<std::string>("--output")
  - Skip clipboard operations in console build (#ifdef PSERV_CONSOLE_BUILD)
  - Write directly to specified file

- [x] **Step 4.8**: Test and verify
  - ✅ All steps 4.1-4.7 tested and working
  - ✅ Service actions (start, stop, restart) confirmed working
  - ✅ Async operations wait correctly
  - ✅ Export actions with --output parameter working
  - ✅ Help text shows available actions
  - ✅ Action dispatch and error handling working

### Phase 5: Command Execution and Output
**Goal**: Wire everything together in pservc.cpp main()
**Status**: NOT STARTED

- [ ] **Step 5.1**: Implement basic command dispatch in main()
  - Parse arguments and identify which subcommand was used
  - Get corresponding controller from DataControllerLibrary
  - Call controller->Refresh() to load data

- [ ] **Step 5.2**: Implement list functionality (default action)
  - Render all objects from controller using console table renderer
  - Apply filters (--filter argument) using substring match
  - Apply sorting (--sort argument) using column name
  - Format based on --format argument (table/json/csv)

- [ ] **Step 5.3**: Implement action commands (Phase 4 dependency)
  - Find matching objects by name/ID from arguments
  - Create dispatch context with selections
  - Execute action via controller
  - Display results

- [ ] **Step 5.4**: Error handling and user feedback
  - Handle invalid arguments gracefully
  - Colored error messages (red) using console.h
  - Success messages (green) using console.h
  - Progress indicators for long operations (if needed)

- [ ] **Step 5.5**: Final testing and commit
  - Test all controllers with list command
  - Test filtering and sorting
  - Test different output formats
  - Test edge cases (no results, invalid filters)
  - Commit command execution

### Phase 6: Polish and Documentation
**Goal**: Make pservc production-ready
**Status**: NOT STARTED

- [ ] **Step 6.1**: Add examples to help text
  - Common use cases in controller `--help` output
  - README section for pservc usage examples

- [ ] **Step 6.2**: Performance optimization (if needed)
  - Lazy loading for large datasets
  - Pagination for long lists (e.g., processes)

- [ ] **Step 6.3**: Integration testing
  - Test pservc alongside pserv5
  - Verify both use same config/logs
  - Test with different log levels

- [ ] **Step 6.4**: Update documentation
  - Update instructions.md to mark Console Variant progress
  - Document console-specific features
  - Add pservc usage guide

## Notes

### Command Examples (Target)
```bash
# List services
pservc services list
pservc services list --status running
pservc services list --format json

# Manage services
pservc services start BITS
pservc services stop "Background Intelligent Transfer Service"
pservc services restart BITS

# Export data
pservc services export --output services.json
pservc processes list --format json > processes.json

# Process management
pservc processes terminate 1234
pservc processes list --filter chrome

# Devices
pservc devices list --enabled-only
```

### Color Scheme
Use console.h macros for coloring:
- **Headers**: `CONSOLE_FOREGROUND_CYAN` (table headers)
- **Running/Active/Highlighted**: `CONSOLE_FOREGROUND_GREEN` (GetVisualState() == Highlighted)
- **Stopped/Disabled**: `CONSOLE_FOREGROUND_GRAY` (GetVisualState() == Disabled)
- **Normal**: `CONSOLE_STANDARD` (GetVisualState() == Normal)
- **Errors**: `CONSOLE_FOREGROUND_RED`
- **Success**: `CONSOLE_FOREGROUND_GREEN`
- **Warnings**: `CONSOLE_FOREGROUND_YELLOW`

### Testing Strategy
- Test each phase before moving to next
- Keep GUI (pserv5) working at all times
- Verify console (pservc) compiles after each step
- Manual testing for user-facing features
- No memory leaks (already verified with --help)

### Current Status (2025-11-24)
**Completed Phases**:
- ✅ Phase 0: Console Infrastructure Setup
- ✅ Phase 2: Console Output Infrastructure
- ✅ Phase 3: Controller Subcommand Registration
- ✅ Phase 4: Action Command Integration
- ✅ Phase 5 (partial): Command Execution and Output

**What Works Now**:
- `pservc services` - Lists all services in sorted table format
- `pservc services --format json` - JSON output
- `pservc services --format csv` - CSV output
- `pservc services --filter <text>` - Filter by text across all fields
- `pservc services --sort <column>` - Sort by column name
- `pservc services --sort <column> --desc` - Sort descending
- `pservc services --col-status running` - Filter by specific column
- `pservc services start <name>` - Start service
- `pservc services stop <name>` - Stop service
- `pservc services restart <name>` - Restart service
- `pservc services uninstall <name> --force` - Destructive action with confirmation
- `pservc services export-to-json --output file.json` - Export with custom args
- Async operations (start/stop) wait correctly
- Proper error handling and colored output
- Unicode/UTF-8 handling with surrogate pairs
- Case-insensitive locale-aware sorting

**Next Steps**:
See "Phase 7: Additional Controller Support" below for detailed plan.

## Phase 7: Additional Controller Support
**Goal**: Add GetAllActions() support for all remaining controllers
**Status**: NOT STARTED

### Controller Support Status:
- ✅ **Services** - COMPLETE (Phase 4)
- ⏭️ **Devices** - Inherits from Services, should work automatically
- ⏭️ **Processes** - Has process_actions.cpp
- ⏭️ **Windows** - Has window_actions.cpp
- ⏭️ **Modules** - Has module_actions.cpp
- ⏭️ **Uninstaller** - Has uninstaller_actions.cpp
- ⏭️ **Startup Programs** - Has startup_program_actions.cpp
- ⏭️ **Scheduled Tasks** - Has scheduled_task_actions.cpp
- ⏭️ **Network Connections** - Has network_connection_actions.cpp
- ⏭️ **Environment Variables** - Has environment_variable_actions.cpp

### Implementation Plan:

**Step 7.1: Verify Devices controller** (TRIVIAL - inherits from Services)
- Test: `pservc devices` - should list devices
- Test: `pservc devices start <name>` - should work (inherited actions)
- Verify: DevicesDataController inherits GetAllActions() from ServicesDataController
- If works: mark complete, no code changes needed

**Step 7.2: Add Process actions support**
- Read: process_actions.cpp to see available actions
- Add: CreateAllProcessActions() function (similar to CreateAllServiceActions)
- Override: GetAllActions() in ProcessesDataController
- Test: `pservc processes`, `pservc processes terminate <pid>`

**Step 7.3: Add Windows actions support**
- Read: window_actions.cpp to see available actions
- Add: CreateAllWindowActions() function
- Override: GetAllActions() in WindowsDataController
- Test: `pservc windows`, `pservc windows close <title>`

**Step 7.4: Add Modules actions support**
- Read: module_actions.cpp to see available actions
- Add: CreateAllModuleActions() function
- Override: GetAllActions() in ModulesDataController
- Test: `pservc modules`

**Step 7.5: Add Uninstaller actions support**
- Read: uninstaller_actions.cpp to see available actions
- Add: CreateAllUninstallerActions() function
- Override: GetAllActions() in UninstallerDataController
- Test: `pservc uninstaller`, `pservc uninstaller uninstall <name> --force`

**Step 7.6: Add Startup Programs actions support**
- Read: startup_program_actions.cpp to see available actions
- Add: CreateAllStartupProgramActions() function
- Override: GetAllActions() in StartupProgramsDataController
- Test: `pservc startup-programs`

**Step 7.7: Add Scheduled Tasks actions support**
- Read: scheduled_task_actions.cpp to see available actions
- Add: CreateAllScheduledTaskActions() function
- Override: GetAllActions() in ScheduledTasksDataController
- Test: `pservc scheduled-tasks`

**Step 7.8: Add Network Connections actions support**
- Read: network_connection_actions.cpp to see available actions
- Add: CreateAllNetworkConnectionActions() function (if any actions exist)
- Override: GetAllActions() in NetworkConnectionsDataController
- Test: `pservc network-connections`

**Step 7.9: Add Environment Variables actions support**
- Read: environment_variable_actions.cpp to see available actions
- Add: CreateAllEnvironmentVariableActions() function
- Override: GetAllActions() in EnvironmentVariablesDataController
- Test: `pservc environment-variables`

**Step 7.10: Final verification**
- Test all 10 controllers with `pservc <controller> --help`
- Verify action epilog shows available actions
- Test at least one action per controller
- Update pservc-plan.md with completion status
