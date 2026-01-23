# pserv5 - Gemini Context & Instructions

## Project Overview
**pserv5** is a modern Windows system administration tool (GUI + CLI) written in C++20. It manages Services, Devices, Processes, Windows, and more. It replaces legacy versions (MFC/C#/WPF) with a native, high-performance architecture.

## Architecture

The project follows a **Model-View-Controller (MVC)** variant, adapted for Immediate Mode GUI.

### Core Components
*   **DataController (`pserv5/core/data_controller.h`)**: The central abstract base class for any "View" (e.g., `ServicesDataController`). It manages:
    *   Data loading (`Refresh()`).
    *   Column definitions (`DataObjectColumn`).
    *   Actions (`GetActions()`).
    *   Visual state (highlighting/disabling).
*   **DataObject (`pserv5/core/data_object.h`)**: Abstract base class for items (e.g., `ServiceInfo`).
    *   Reference counted (`RefCountImpl`).
    *   Provides string properties for display and typed properties for sorting.
*   **Managers (`pserv5/windows_api/`)**: Wrappers around Win32 APIs (e.g., `ServiceManager`).
    *   Use **WIL** (`wil::unique_schandle`, etc.) for resource management.
    *   Separate static methods for control (Start/Stop) from instance methods for enumeration.
*   **MainWindow (`pserv5/main_window.cpp`)**:
    *   Manages the Win32 window and DirectX 11 swapchain.
    *   Initializes ImGui.
    *   Maintains a list of Controllers.
    *   Delegates rendering to the active Controller.
*   **CLI (`pserv5/pservc/pservc.cpp`)**:
    *   Separate entry point.
    *   Reuses `DataController` logic.
    *   Uses `ConsoleTable` to render `DataObject`s to stdout.

### Key Patterns
*   **Async Operations**: Long-running tasks use `AsyncOperation` and report progress via `WM_ASYNC_OPERATION_COMPLETE`.
*   **Smart Pointers**: Heavy use of `std::shared_ptr`, `std::unique_ptr`, and WIL resource wrappers.
*   **Configuration**: Settings stored in `pserv5.toml` via `toml++` and `config/settings.h`.
*   **Logging**: `spdlog` used throughout (`spdlog::info`, `spdlog::error`).

## Technology Stack
*   **Language**: C++20
*   **GUI**: [Dear ImGui](https://github.com/ocornut/imgui) (Docking branch)
*   **Renderer**: DirectX 11
*   **Windows API**: [WIL (Windows Implementation Library)](https://github.com/microsoft/wil)
*   **Logging**: [spdlog](https://github.com/gabime/spdlog)
*   **Config**: [toml++](https://github.com/marzer/tomlplusplus)
*   **CLI Parsing**: [argparse](https://github.com/p-ranav/argparse)
*   **JSON Export**: [RapidJSON](https://github.com/Tencent/rapidjson)

## Developer Instructions

### 1. Build Process (CRITICAL)
*   **DO NOT ATTEMPT TO BUILD MANUALLY.**
*   MSBuild via CLI is unreliable for this solution.
*   **Action**: Make code changes -> Verify logic/syntax -> Inform user "Changes are ready for testing."

### 2. Code Style & Conventions
*   **C++ Standard**: C++20 (concepts, ranges, `std::format`).
*   **Formatting**: Follow `.clang-format`.
*   **Naming**: PascalCase for methods/types, camelCase for variables, `m_` prefix for members.
*   **Error Handling**: Use exceptions for operational failures (e.g., `std::runtime_error` in Managers). Use `spdlog` for reporting.
*   **Resource Management**: **ALWAYS** use WIL (`wil::unique_handle`, `wil::unique_schandle`) for Win32 handles. Avoid raw `CloseHandle`.

### 3. Adding New Features
*   **New View**:
    1.  Create `Model` inheriting `DataObject`.
    2.  Create `Manager` wrapping Win32 APIs.
    3.  Create `Controller` inheriting `DataController`.
    4.  Register in `DataControllerLibrary`.
*   **New Action**:
    1.  Create class inheriting `DataAction`.
    2.  Implement `Execute(DataActionDispatchContext& ctx)`.
    3.  Register in Controller's `GetActions`.

### 4. File Operations
*   Always `read_file` relevant headers before modifying implementation files.
*   Respect `precomp.h` - it speeds up compilation significantly.

## Directory Structure
*   `pserv5/`
    *   `actions/` - Command pattern implementations.
    *   `config/` - Configuration backend.
    *   `controllers/` - UI/Data logic glue.
    *   `core/` - Base classes (`DataController`, `DataObject`).
    *   `models/` - Data containers (`ServiceInfo`).
    *   `pservc/` - CLI specific code.
    *   `utils/` - Helpers (Logging, StringUtils).
    *   `windows_api/` - Low-level Win32 wrappers.
