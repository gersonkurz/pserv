# pservc - Command Line Interface

`pservc.exe` is the command-line interface for pserv5. It provides full access to all system management features without a graphical interface, making it ideal for scripts, automation, and remote administration.

## Quick Start

```bash
# List all services
pservc services

# List running processes
pservc processes

# Start a service
pservc services start Spooler

# Stop a service (destructive actions require --force)
pservc services stop Spooler --force

# Export services to JSON
pservc services --format json > services.json
```

## General Syntax

```
pservc <command> [options]
pservc <command> <action> <target(s)> [options]
```

Where:
- `<command>` is one of the available commands (services, processes, etc.)
- `<action>` is an optional action to perform (start, stop, terminate, etc.)
- `<target(s)>` are the object(s) to act upon (by name)
- `[options]` are command-specific options

## Available Commands

| Command | Description |
|---------|-------------|
| `services` | Manage Windows services |
| `devices` | Browse hardware devices |
| `processes` | Monitor running processes |
| `windows` | Enumerate application windows |
| `modules` | Inspect loaded DLLs (requires process selection) |
| `uninstaller` | Browse installed programs |
| `startup-programs` | Manage startup applications |
| `scheduled-tasks` | View scheduled tasks |
| `network-connections` | Monitor TCP/UDP connections |
| `environment-variables` | View/edit environment variables |

## Common Options

These options are available for all commands when listing data:

| Option | Description |
|--------|-------------|
| `--format <fmt>` | Output format: `table` (default), `json`, or `csv` |
| `--filter <text>` | Filter results by text (case-insensitive, searches all fields) |
| `--sort <column>` | Sort by column name |
| `--desc` | Sort in descending order (use with `--sort`) |
| `--col-<name> <text>` | Filter by specific column (e.g., `--col-status Running`) |
| `-h`, `--help` | Show help for the command |

## Output Formats

### Table (default)

Human-readable table format with aligned columns:

```
$ pservc services --filter spooler
Name     Display Name   Status   Start Type
Spooler  Print Spooler  Running  Automatic
```

### JSON

Machine-readable JSON format, useful for scripting:

```bash
$ pservc services --filter spooler --format json
[
  {
    "Name": "Spooler",
    "DisplayName": "Print Spooler",
    "Status": "Running",
    "StartType": "Automatic",
    ...
  }
]
```

### CSV

Comma-separated values for spreadsheet import:

```bash
$ pservc services --format csv > services.csv
```

---

## Services

Manage Windows services.

### Listing Services

```bash
# List all services
pservc services

# Filter by status
pservc services --col-status Running

# Filter by start type
pservc services --col-starttype Automatic

# Search by name
pservc services --filter print

# Sort by display name
pservc services --sort "Display Name"

# Export to JSON
pservc services --format json
```

### Available Columns

| Column | Description |
|--------|-------------|
| `Name` | Service name (internal identifier) |
| `DisplayName` | Human-readable display name |
| `Status` | Current state (Running, Stopped, etc.) |
| `StartType` | Startup type (Automatic, Manual, Disabled) |
| `ProcessId` | Process ID if running |
| `ServiceType` | Type (Win32 Own Process, Share Process, etc.) |
| `BinaryPath` | Path to executable |
| `Description` | Service description |

### Service Actions

| Action | Description | Destructive |
|--------|-------------|-------------|
| `start` | Start a stopped service | No |
| `stop` | Stop a running service | Yes |
| `restart` | Stop then start a service | Yes |
| `pause` | Pause a running service | Yes |
| `continue` | Resume a paused service | No |
| `delete` | Delete a service permanently | Yes |

#### Examples

```bash
# Start the Print Spooler service
pservc services start Spooler

# Stop multiple services
pservc services stop Spooler "Print Spooler" --force

# Restart a service
pservc services restart Spooler --force

# Delete a service (DANGEROUS!)
pservc services delete MyService --force
```

---

## Processes

Monitor and manage running processes.

### Listing Processes

```bash
# List all processes
pservc processes

# Filter by name
pservc processes --filter chrome

# Filter by user
pservc processes --col-user "NT AUTHORITY\\SYSTEM"

# Sort by memory usage (descending)
pservc processes --sort "Working Set" --desc
```

### Available Columns

| Column | Description |
|--------|-------------|
| `Name` | Process name |
| `ProcessId` | Process ID (PID) |
| `User` | User account running the process |
| `WorkingSet` | Memory usage |
| `Priority` | Process priority |
| `Path` | Executable path |
| `CommandLine` | Full command line |

### Process Actions

| Action | Description | Destructive |
|--------|-------------|-------------|
| `terminate` | Forcefully terminate a process | Yes |
| `set-priority-high` | Set process priority to High | No |
| `set-priority-normal` | Set process priority to Normal | No |
| `set-priority-low` | Set process priority to Low | No |

#### Examples

```bash
# Terminate a process by name
pservc processes terminate notepad.exe --force

# Terminate by PID (use the Name column which shows the process name)
pservc processes terminate notepad.exe --force
```

---

## Devices

Browse hardware devices.

### Listing Devices

```bash
# List all devices
pservc devices

# Filter by status
pservc devices --col-status OK

# Search for specific device
pservc devices --filter keyboard
```

### Available Columns

| Column | Description |
|--------|-------------|
| `Name` | Device name |
| `Class` | Device class (Display, Keyboard, etc.) |
| `Status` | Device status |
| `Manufacturer` | Device manufacturer |
| `Driver` | Driver name |

### Device Actions

| Action | Description | Destructive |
|--------|-------------|-------------|
| `enable` | Enable a disabled device | No |
| `disable` | Disable a device | Yes |

---

## Windows

Enumerate and interact with application windows.

### Listing Windows

```bash
# List all windows
pservc windows

# Filter by visibility
pservc windows --col-visible Yes

# Search by title
pservc windows --filter "Visual Studio"
```

### Available Columns

| Column | Description |
|--------|-------------|
| `Title` | Window title |
| `Handle` | Window handle (HWND) |
| `Class` | Window class name |
| `ProcessId` | Owning process ID |
| `Visible` | Whether window is visible |

---

## Modules

Inspect loaded DLLs and modules for a process.

```bash
# List modules for explorer.exe
pservc modules --filter explorer
```

---

## Uninstaller

Browse and manage installed programs.

### Listing Installed Programs

```bash
# List all installed programs
pservc uninstaller

# Search by name
pservc uninstaller --filter "Microsoft"

# Export to JSON
pservc uninstaller --format json
```

### Available Columns

| Column | Description |
|--------|-------------|
| `DisplayName` | Program name |
| `Publisher` | Publisher/vendor |
| `Version` | Installed version |
| `InstallDate` | Installation date |
| `InstallLocation` | Installation path |

### Uninstaller Actions

| Action | Description | Destructive |
|--------|-------------|-------------|
| `uninstall` | Run the program's uninstaller | Yes |

---

## Startup Programs

Manage applications that run at system startup.

### Listing Startup Programs

```bash
# List all startup programs
pservc startup-programs

# Filter by location (registry vs folder)
pservc startup-programs --col-type Registry
```

### Available Columns

| Column | Description |
|--------|-------------|
| `Name` | Program name |
| `Command` | Command that runs at startup |
| `Location` | Registry key or folder path |
| `Type` | Registry or StartupFolder |
| `Scope` | User or System |
| `Enabled` | Whether currently enabled |

### Startup Program Actions

| Action | Description | Destructive |
|--------|-------------|-------------|
| `enable` | Enable a startup program | No |
| `disable` | Disable a startup program | No |
| `delete` | Remove from startup | Yes |

---

## Scheduled Tasks

View Windows scheduled tasks.

### Listing Scheduled Tasks

```bash
# List all scheduled tasks
pservc scheduled-tasks

# Filter by state
pservc scheduled-tasks --col-state Ready
```

### Available Columns

| Column | Description |
|--------|-------------|
| `Name` | Task name |
| `Path` | Task path in scheduler |
| `State` | Current state (Ready, Running, Disabled) |
| `LastRunTime` | Last execution time |
| `NextRunTime` | Next scheduled run |
| `Triggers` | Trigger summary |

### Scheduled Task Actions

| Action | Description | Destructive |
|--------|-------------|-------------|
| `run` | Run the task immediately | No |
| `enable` | Enable a disabled task | No |
| `disable` | Disable a task | No |
| `delete` | Delete the task | Yes |

---

## Network Connections

Monitor active network connections.

### Listing Connections

```bash
# List all connections
pservc network-connections

# Filter by state
pservc network-connections --col-state ESTABLISHED

# Filter by protocol
pservc network-connections --col-protocol TCP
```

### Available Columns

| Column | Description |
|--------|-------------|
| `Protocol` | TCP or UDP |
| `LocalAddress` | Local IP:port |
| `RemoteAddress` | Remote IP:port |
| `State` | Connection state |
| `ProcessId` | Owning process ID |
| `ProcessName` | Owning process name |

---

## Environment Variables

View and manage environment variables.

### Listing Environment Variables

```bash
# List all environment variables
pservc environment-variables

# Filter by scope (User or System)
pservc environment-variables --col-scope User

# Search by name
pservc environment-variables --filter PATH
```

### Available Columns

| Column | Description |
|--------|-------------|
| `Name` | Variable name |
| `Value` | Variable value |
| `Scope` | User or System |

### Environment Variable Actions

| Action | Description | Destructive |
|--------|-------------|-------------|
| `delete` | Delete an environment variable | Yes |

---

## Scripting Examples

### Export all services to JSON

```bash
pservc services --format json > all-services.json
```

### Find all stopped automatic services

```bash
pservc services --col-status Stopped --col-starttype Automatic
```

### Start all stopped automatic services

```powershell
# PowerShell example
$services = pservc services --col-status Stopped --col-starttype Automatic --format json | ConvertFrom-Json
foreach ($svc in $services) {
    pservc services start $svc.Name
}
```

### Monitor running processes sorted by memory

```bash
pservc processes --sort "Working Set" --desc --format table
```

### Export network connections to CSV

```bash
pservc network-connections --format csv > connections.csv
```

### Find all Chrome-related processes

```bash
pservc processes --filter chrome --format json
```

---

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Error (message printed to stderr) |

---

## Notes

- **Destructive actions** require the `--force` flag to confirm intent
- **Target names** are matched case-insensitively against the first column (usually Name)
- **Multiple targets** can be specified for actions: `pservc services start Svc1 Svc2 Svc3`
- **Administrator privileges** may be required for certain operations (starting/stopping services, terminating processes, etc.)
- **Remote machines**: The console tool operates on the local machine only. Use the GUI (`pserv5.exe`) for remote machine access.
