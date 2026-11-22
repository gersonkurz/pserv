<p align="center">
  <img src="logo.jpg" alt="pserv logo" width="1024"/>
</p>

A Windows system administration tool for managing services, devices, processes, and more. Native C++ application with a WinUI 3 (Fluent) interface, and a command-line companion, `pservc`.

More than 10 years after the last iteration, I decided to rewrite an old tool of mine originally started in 1998. [Click here to find out more about the history of the product](https://p-nand-q.com/download/pserv_cpl/index.html).

## Manual

[Instructions for the GUI](docs/pserv6.md) and [for the command line interface](docs/pservc.md).

## Screenshot

![Screenshot of pserv6](docs/pserv6.png)

## Features

- **Services** - View, start, stop, configure Windows services
- **Devices** - Browse and manage hardware devices
- **Processes** - Monitor running processes with detailed information
- **Windows** - Enumerate and interact with application windows
- **Modules** - Inspect loaded DLLs and modules
- **Uninstaller** - Browse and remove installed programs
- **Startup Programs** - Manage applications that run at startup
- **Scheduled Tasks** - View and control Windows scheduled tasks
- **Network Connections** - Monitor active TCP/UDP connections
- **Environment Variables** - View and edit system/user environment variables

## Building

From a Visual Studio 2026 Developer shell, with [just](https://github.com/casey/just) installed:

```
just build      # Debug, native architecture (x64 or ARM64)
just run        # build and start pserv6 (it starts unelevated; "Run as administrator" in the window elevates)
just test       # build and run the core tests
just release    # Release builds, MSI installers (needs MSIS 3.x) and the bundle
```

`just --list` shows all recipes. The GUI uses the Windows App SDK (NuGet, restored by the build).
Other dependencies are included as git submodules:
- [spdlog](https://github.com/gabime/spdlog) - Logging
- [WIL](https://github.com/microsoft/wil) - Windows Implementation Library
- [toml++](https://github.com/marzer/tomlplusplus) - Configuration
- [RapidJSON](https://github.com/Tencent/rapidjson) - JSON export
- [argparse](https://github.com/p-ranav/argparse) - Command-line parsing

If you `git checkout` a different commit, remember to update submodules:
```
git submodule update --init --recursive
```

## History

pserv has been around since 1998:

| Version | Year | Technology |
|---------|------|------------|
| v1 | 1998 | Custom Windows library |
| v2 | 2002 | MFC |
| v3 | 2010 | C# Windows Forms |
| v4 | 2014 | C# WPF with MahApps.Metro |
| v5 | 2025 | C++20 with ImGui |
| v6 | 2026 | C++ with WinUI 3 |

## License

[MIT](LICENSE)
