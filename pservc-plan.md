# pservc Console Implementation Plan

## Overview
Implement a full-featured console interface for pserv5 that exposes all controller functionality via command-line arguments.

## Architecture
- **Command Structure**: Controller as subcommand (e.g., `pservc services list`, `pservc processes terminate 1234`)
- **Output Format**: Colored terminal output using fmt library
- **Actions**: Map DataAction objects to subcommands with arguments

## Tasks

### Phase 1: Switch to fmt::format (TODO #5)
**Goal**: Replace all `std::format` with `fmt::format` to enable colored console output

- [ ] **Step 1.1**: Add fmt library integration
  - Check if spdlog's bundled fmt is accessible
  - If not, add fmt as a submodule or use spdlog's bundled version
  - Update precomp.h to include fmt headers

- [ ] **Step 1.2**: Replace std::format calls (51 occurrences across 20 files)
  - Replace `std::format` → `fmt::format`
  - Replace `std::format_to` → `fmt::format_to` (if any)
  - Test compilation after each file or group of files

- [ ] **Step 1.3**: Verify and commit
  - Build both pserv5 (GUI) and pservc (console)
  - Ensure no regressions
  - Commit changes

### Phase 2: Console Output Infrastructure
**Goal**: Create formatters to display data objects in the terminal

- [ ] **Step 2.1**: Create console output utilities
  - `console_formatter.h/cpp`: Base formatting utilities
  - Color scheme definitions (using fmt colors)
  - Table rendering for data objects
  - Support for filtering/sorting display

- [ ] **Step 2.2**: Implement DataObject console rendering
  - Method to render DataObject as table row
  - Method to render collection as formatted table
  - Handle column widths, alignment
  - Support for different output formats (table, json, plain)

- [ ] **Step 2.3**: Verify and commit
  - Test with sample data
  - Commit console output infrastructure

### Phase 3: Controller Subcommand Registration
**Goal**: Implement `RegisterArguments()` for each controller

- [ ] **Step 3.1**: Design subcommand structure
  - Each controller registers as subcommand (e.g., "services", "processes")
  - Common arguments: `--format`, `--filter`, `--sort`
  - Controller-specific arguments based on columns

- [ ] **Step 3.2**: Implement base RegisterArguments() pattern
  - Update `DataController::RegisterArguments()` to be virtual
  - Add common arguments (format, filter, sort)
  - Add "list" subcommand for all controllers

- [ ] **Step 3.3**: Implement controller-specific arguments
  - Services: `--type` (service type filter), `--status` (running/stopped)
  - Processes: `--pid`, `--name`
  - Other controllers: relevant filters

- [ ] **Step 3.4**: Verify and commit
  - Test argument parsing for each controller
  - `pservc services --help` should show options
  - Commit subcommand registration

### Phase 4: Action Command Integration
**Goal**: Map DataAction objects to executable console commands

- [ ] **Step 4.1**: Design action command mapping
  - Each action becomes a subcommand: `pservc services start <name>`
  - Actions requiring input: add arguments (e.g., `--file` for export)
  - Handle multi-selection (pass multiple targets)

- [ ] **Step 4.2**: Extend RegisterArguments() for actions
  - Query controller's GetActions() for available actions
  - Register each action as subcommand
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

- [ ] **Step 5.1**: Implement command dispatch in main()
  - Parse arguments and identify controller + action
  - Call controller->Refresh() to load data
  - Execute requested action or display list

- [ ] **Step 5.2**: Implement list command
  - Render all objects from controller
  - Apply filters and sorting
  - Format as table with colors

- [ ] **Step 5.3**: Implement action commands
  - Find matching objects by name/ID
  - Create dispatch context with selections
  - Execute action
  - Display results

- [ ] **Step 5.4**: Error handling and user feedback
  - Handle invalid arguments gracefully
  - Colored error messages (red)
  - Success messages (green)
  - Progress indicators for long operations

- [ ] **Step 5.5**: Final testing and commit
  - Test all controllers and actions
  - Test edge cases (no results, invalid input)
  - Update documentation
  - Final commit

### Phase 6: Polish and Documentation
**Goal**: Make pservc production-ready

- [ ] **Step 6.1**: Add examples to help text
  - Common use cases in `--help`
  - README section for pservc usage

- [ ] **Step 6.2**: Performance optimization
  - Lazy loading for large datasets
  - Pagination for long lists

- [ ] **Step 6.3**: Integration testing
  - Test pservc alongside pserv5
  - Verify both use same config/logs
  - Test remote machine connections

- [ ] **Step 6.4**: Update instructions.md
  - Mark Console Variant as complete
  - Document console-specific features

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

### Color Scheme (Proposed)
- **Headers**: Cyan/Bold
- **Running/Active**: Green
- **Stopped/Disabled**: Gray
- **Errors**: Red
- **Success**: Green
- **Warnings**: Yellow

### Testing Strategy
- Test each step before moving to next
- Keep GUI (pserv5) working at all times
- Verify console (pservc) compiles after each step
- Manual testing for user-facing features
