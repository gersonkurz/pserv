<p align="center">
  <img src="logo.jpg" alt="pserv logo" width="1024"/>
</p>

A Windows system administration tool for managing services, devices, processes, and more. Native C++20 application with a modern ImGui interface.

More than 10 years after the last iteration, I decided to rewrite an old tool of mine originally started in 1998. [Click here to find out more about the history of the product](https://p-nand-q.com/download/pserv_cpl/index.html).

## Manual

[Brief instructions for the UI](docs/pserv5.md) and [for the command line interface](docs/pservc.md).

## Screenshot

![Screenshot of main application](docs/main.png)

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

Open `pserv5.sln` in Visual Studio 2022 and build the solution.

Dependencies are included as git submodules:
- [ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI
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

## License

[MIT](LICENSE)
