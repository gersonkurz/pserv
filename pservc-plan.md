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
**Status**: NOT STARTED - actions will come after basic list functionality works

- [ ] **Step 4.1**: Design action command mapping
  - Decide: actions as subcommands (`pservc services start <name>`) vs verbs?
  - Actions requiring input: add arguments (e.g., `--output <file>` for export)
  - Handle multi-selection (pass multiple targets)

- [ ] **Step 4.2**: Extend RegisterArguments() for actions
  - Query controller's GetActions() for available actions
  - Register each action as subcommand or verb
  - Add action-specific arguments based on action type

- [ ] **Step 4.3**: Implement action execution
  - Create console DataActionDispatchContext
  - Execute action with provided arguments
  - Handle progress reporting for async operations
  - Report success/failure with colored output

- [ ] **Step 4.4**: Handle actions requiring user input
  - Export actions: `--output <file>` argument
  - Delete actions: `--force` to skip confirmation
  - Interactive prompts when arguments missing

- [ ] **Step 4.5**: Verify and commit
  - Test each action type (start/stop/delete/export)
  - Test error handling
  - Commit action execution

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
**Completed**:
- Console infrastructure with ANSI colors
- Logging configuration (spdlog debug+file only)
- Subcommand registration with argparse
- Memory leak fixes
- Console table rendering (table/json/csv formats)
- UTF-8 and Unicode handling (umlauts, emojis, surrogate pairs)
- Case-insensitive locale-aware sorting
- Command execution and list functionality

**What Works Now**:
- `pservc services` - Lists all services in sorted table format
- `pservc services --format json` - JSON output
- `pservc services --format csv` - CSV output
- Proper Unicode handling in all output formats
- Clean table alignment with color coding

**Next Session**:
- Implement --filter and --sort command-line arguments
- Or start Phase 4 (Action Command Integration) for service start/stop/etc
