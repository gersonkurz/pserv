# pserv6: native C++/WinRT UI and shared-core CLI. VS 2026 Developer shell required.
set windows-shell := ["cmd.exe", "/c"]
platform := `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/native-platform.ps1`
bindir := if platform == "x86" { "Win32" } else { platform }

default:
    @just --list

[private]
_require-devshell:
    @if not "%VisualStudioVersion%"=="18.0" (echo ERROR: Open a Visual Studio 2026 Developer shell before building. & exit /b 1)

# Restore pinned Windows App SDK 2.7 and C++/WinRT dependencies.
restore-winui:
    nuget restore pserv6\ui\packages.config -PackagesDirectory pserv6\ui\packages -NonInteractive

# Build GUI and CLI, Debug, native OS architecture.
build: (build-winui platform)

# Build GUI and CLI, Release, native OS architecture.
build-release: (build-winui-release platform)

# Explicit architecture: ARM64, x64 or x86.
build-winui arch=platform: _require-devshell restore-winui
    msbuild pserv6\pserv6.slnx /p:Configuration=Debug /p:Platform={{arch}} /m /nologo /v:minimal

build-winui-release arch=platform: _require-devshell restore-winui
    msbuild pserv6\pserv6.slnx /p:Configuration=Release /p:Platform={{arch}} /m /nologo /v:minimal

build-all: (build-winui "x64") (build-winui-release "x64") (build-winui "x86") (build-winui-release "x86") (build-winui "ARM64") (build-winui-release "ARM64")

# Build and launch native Release.
run: build-release
    @pserv6\bin\{{bindir}}\Release\pserv6.exe

# Build and launch native Debug.
run-debug: build
    @pserv6\bin\{{bindir}}\Debug\pserv6.exe

run-cli *ARGS: build-release
    @pserv6\bin\{{bindir}}\Release\pserv6c.exe {{ARGS}}

# Regression checks; use a Developer shell targeting the native OS architecture.
test: build-release
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-service-edits.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-environment-edits.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-winui-rows.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-winui-cell-collection.ps1 -Platform {{platform}}
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-keyboard-shortcuts.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test-exporter-link.ps1 -Platform {{platform}}

rebuild: clean build

clean:
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\clean.ps1

version VERSION:
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\set-version.ps1 {{VERSION}}

# Build all three MSIs AND the final architecture-selecting bundle.
package-all: package-x64 package-x86 package-arm64 bundle
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\verify-release.ps1

release: package-all

package-x64: (_stage "x64") (_package "pserv6-x64.msis")

package-x86: (_stage "x86") (_package "pserv6-x86.msis")

package-arm64: (_stage "ARM64") (_package "pserv6-arm64.msis")

# Combine existing MSIs into setup/pserv<VERSION>.exe.
bundle:
    msis /BUILD setup\setup-bundle.msis

[private]
_stage arch: (build-winui-release arch)
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\stage-release.ps1 {{arch}}

[private]
_package script:
    @cd setup&& msis /BUILD /STANDALONE {{script}}
