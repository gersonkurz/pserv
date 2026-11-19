# pserv6c - Command Line Interface

`pserv6c.exe` is the command-line interface for pserv6. It exposes the ten local providers and their registered actions for scripts and automation. GUI-specific editing dialogs and remote SCM connections are available in the graphical application.

## Quick Start

```bash
# List all services
pserv6c services

# List running processes
pserv6c processes

# Start a service
pserv6c services start Spooler

# Stop a service (destructive actions require --force)
pserv6c services stop Spooler --force

# Export services to JSON
pserv6c services --format json > services.json
```

## General Syntax

```
pserv6c <command> [options]
pserv6c <command> <action> <target(s)> [options]
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
$ pserv6c services --filter spooler
Name     Display Name   Status   Start Type
Spooler  Print Spooler  Running  Automatic
```

### JSON

Machine-readable JSON format, useful for scripting:

```bash
$ pserv6c services --filter spooler --format json
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
$ pserv6c services --format csv > services.csv
```

---

## Services

Manage Windows services.

### Listing Services

```bash
# List all services
pserv6c services

# Filter by status
pserv6c services --col-status Running

# Filter by start type
pserv6c services --col-starttype Automatic

# Search by name
pserv6c services --filter print

# Sort by display name
pserv6c services --sort "Display Name"

# Export to JSON
pserv6c services --format json
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
pserv6c services start Spooler

# Stop multiple services
pserv6c services stop Spooler "Print Spooler" --force

# Restart a service
pserv6c services restart Spooler --force

# Delete a service (DANGEROUS!)
pserv6c services delete MyService --force
```

---

## Processes

Monitor and manage running processes.

### Listing Processes

```bash
# List all processes
pserv6c processes

# Filter by name
pserv6c processes --filter chrome

# Filter by user
pserv6c processes --col-user "NT AUTHORITY\\SYSTEM"

# Sort by memory usage (descending)
pserv6c processes --sort "Working Set" --desc
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
pserv6c processes terminate notepad.exe --force

# Terminate by PID (use the Name column which shows the process name)
pserv6c processes terminate notepad.exe --force
```

---

## Devices

Browse hardware devices.

### Listing Devices

```bash
# List all devices
pserv6c devices

# Filter by status
pserv6c devices --col-status OK

# Search for specific device
pserv6c devices --filter keyboard
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
pserv6c windows

# Filter by visibility
pserv6c windows --col-visible Yes

# Search by title
pserv6c windows --filter "Visual Studio"
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
pserv6c modules --filter explorer
```

---

## Uninstaller

Browse and manage installed programs.

### Listing Installed Programs

```bash
# List all installed programs
pserv6c uninstaller

# Search by name
pserv6c uninstaller --filter "Microsoft"

# Export to JSON
pserv6c uninstaller --format json
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
pserv6c startup-programs

# Filter by location (registry vs folder)
pserv6c startup-programs --col-type Registry
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
pserv6c scheduled-tasks

# Filter by state
pserv6c scheduled-tasks --col-state Ready
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
pserv6c network-connections

# Filter by state
pserv6c network-connections --col-state ESTABLISHED

# Filter by protocol
pserv6c network-connections --col-protocol TCP
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
pserv6c environment-variables

# Filter by scope (User or System)
pserv6c environment-variables --col-scope User

# Search by name
pserv6c environment-variables --filter PATH
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
pserv6c services --format json > all-services.json
```

### Find all stopped automatic services

```bash
pserv6c services --col-status Stopped --col-starttype Automatic
```

### Start all stopped automatic services

```powershell
# PowerShell example
$services = pserv6c services --col-status Stopped --col-starttype Automatic --format json | ConvertFrom-Json
foreach ($svc in $services) {
    pserv6c services start $svc.Name
}
```

### Monitor running processes sorted by memory

```bash
pserv6c processes --sort "Working Set" --desc --format table
```

### Export network connections to CSV

```bash
pserv6c network-connections --format csv > connections.csv
```

### Find all Chrome-related processes

```bash
pserv6c processes --filter chrome --format json
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
- **Multiple targets** can be specified for actions: `pserv6c services start Svc1 Svc2 Svc3`
- **Administrator privileges** may be required for certain operations (starting/stopping services, terminating processes, etc.)
- **Remote machines**: The console tool operates on the local machine only. Use the GUI (`pserv6.exe`) for remote machine access.
`--format json` and `--format csv` produce UTF-8 when redirected. Progress and the version banner go to stderr, so stdout can be piped directly to a parser.
