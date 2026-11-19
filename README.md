<p align="center">
  <img src="logo.jpg" alt="pserv logo" width="1024"/>
</p>

A Windows system administration tool for managing services, drivers, processes, and more. pserv6 uses native C++/WinRT and WinUI 3, with a shared-core command-line frontend, pserv6c.

More than 10 years after the last iteration, I decided to rewrite an old tool of mine originally started in 1998. [Click here to find out more about the history of the product](https://p-nand-q.com/download/pserv_cpl/index.html).

## Manual

[GUI manual](docs/pserv6.md), [command-line manual](docs/pserv6c.md), and [release review](docs/release-review.md).

## Features

- **Services** - View, start, stop, configure Windows services
- **Devices** - Browse and manage driver services
- **Processes** - Monitor running processes with detailed information
- **Windows** - Enumerate and interact with application windows
- **Modules** - Inspect loaded DLLs and modules
- **Uninstaller** - Browse and remove installed programs
- **Startup Programs** - Manage applications that run at startup
- **Scheduled Tasks** - View and control Windows scheduled tasks
- **Network Connections** - Monitor active TCP/UDP connections
- **Environment Variables** - View and edit system/user environment variables

## Building

Use a Visual Studio 2026 Developer shell with v145 C++ tools, Windows SDK 10.0.26100.0, `just`, and `nuget` on PATH.

```powershell
just build-release # Build pserv6 Release for the native OS architecture
just run           # Build and launch pserv6 Release
```

ARM64 hosts select ARM64 even from an emulated x64 shell. Both executables and their runtime files are under `pserv6/bin/ARM64/Release/` on ARM64. Open `pserv6/pserv6.slnx` in Visual Studio. `just build` builds Debug; `just run-debug` launches Debug. `just test` runs the repository regression checks.

`just package-all` builds x64, x86 and ARM64 Release installers **and the final bundle**, `setup/pserv6.0.0.exe` for the current version. Packaging requires MSIS 3.x. `just bundle` rebuilds only the bundle from existing MSIs. The Release payload includes the matching app-local v145 CRT and Windows App SDK runtime. Distribute the full output, not just the EXE.

Dependencies are included as git submodules:
- [spdlog](https://github.com/gabime/spdlog) - Logging
- [WIL](https://github.com/microsoft/wil) - Windows Implementation Library
- [toml++](https://github.com/marzer/tomlplusplus) - Configuration
- [RapidJSON](https://github.com/Tencent/rapidjson) - JSON export
- [argparse](https://github.com/p-ranav/argparse) - Command-line parsing
- [fkYAML](https://github.com/fktn-k/fkYAML) - Base16 palette parsing

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
| v6 | In development | Native C++/WinRT with WinUI 3 |

## License

[MIT](LICENSE)
