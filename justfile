# Build automation for pserv6 — native C++/WinRT and WinUI 3.
# The pserv5 ImGui frontend and shared-core CLI remain available explicitly.
# Requires: just, msbuild on PATH (VS 2026 Developer Command Prompt / pwsh).
#
# Builds drive off the solution so $(SolutionDir) resolves correctly — the
# projects derive OutDir/IntDir from it. Output lands in pserv5\bin\<Platform>\<Config>\.
# Existing MSI recipes package pserv5; pserv6 packaging is not implemented yet.

set windows-shell := ["cmd.exe", "/c"]

solution := "pserv5\\pserv5.slnx"

# Query Windows, not the architecture of an emulated just or Developer shell.
platform := `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/native-platform.ps1`
bindir := if platform == "x86" { "Win32" } else { platform }

# Default: show available recipes
default:
    @just --list

# Fail early with a clear message when not run from a VS 2026 Developer shell.
# A Developer Command Prompt / Developer PowerShell sets VisualStudioVersion and
# puts msbuild on PATH; without it the build fails cryptically.
[private]
_require-devshell:
    @if not "%VisualStudioVersion%"=="18.0" (echo. & echo ERROR: This needs a Visual Studio 2026 Developer shell. & echo Open "Developer Command Prompt for VS 2026" or "Developer PowerShell for VS 2026", then retry. & echo Expected VisualStudioVersion=18.0 but found "%VisualStudioVersion%". & exit /b 1)

# Run MSBuild for one configuration/platform against the solution.
[private]
_msbuild configuration platform: _require-devshell
    msbuild {{solution}} /p:Configuration={{configuration}} /p:Platform={{platform}} /m /nologo /v:minimal

# Build pserv6 Debug for the native OS architecture.
build: (build-winui platform)

# Build Release (native arch)
build-release: (build-winui-release platform)

# Build everything: Debug+Release across x64, x86, ARM64
build-all: (build-winui "x64") (build-winui-release "x64") (build-winui "x86") (build-winui-release "x86") (build-winui "ARM64") (build-winui-release "ARM64")

# Build and launch pserv6 Release (native architecture).
run: build-release
    @pserv5\bin\{{bindir}}\Release\pserv6\pserv6.exe

# Build the original ImGui frontend and CLI (Debug, native architecture).
build-legacy: (_msbuild "Debug" platform)

# Build the original ImGui frontend and CLI (Release, native architecture).
build-legacy-release: (_msbuild "Release" platform)

# Build and launch the original ImGui frontend.
run-legacy: build-legacy
    @pserv5\bin\{{bindir}}\Debug\pserv5.exe

# Build Debug + launch the CLI (native arch); pass args after `--`, e.g. `just run-cli -- services`
run-cli *ARGS: build-legacy
    @pserv5\bin\{{bindir}}\Debug\pservc.exe {{ARGS}}

# Restore the pinned WinUI 3 frontend dependencies.
restore-winui:
    nuget restore pserv5\pserv6\packages.config -PackagesDirectory pserv5\pserv6\packages -NonInteractive

# Build the separate WinUI frontend (Windows App SDK 2.7).
build-winui arch=platform: _require-devshell restore-winui
    msbuild pserv5\pserv6.slnx /p:Configuration=Debug /p:Platform={{arch}} /m /nologo /v:minimal

# Build the WinUI frontend in Release.
build-winui-release arch=platform: _require-devshell restore-winui
    msbuild pserv5\pserv6.slnx /p:Configuration=Release /p:Platform={{arch}} /m /nologo /v:minimal

# Build and launch the separate WinUI frontend.
run-winui arch=platform: (build-winui arch)
    @pserv5\bin\{{if arch == "x86" { "Win32" } else { arch }}}\Debug\pserv6\pserv6.exe

# Clean + rebuild Debug (native arch)
rebuild: clean build

# Clean all build + packaging output (bin\, temp\, dist\)
clean:
    if exist pserv5\bin rmdir /s /q pserv5\bin
    if exist pserv5\temp rmdir /s /q pserv5\temp
    if exist dist rmdir /s /q dist

# Set the version (X.Y.Z) in version.h and every setup\*.msis (pserv5.rc reads version.h).
version VERSION:
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\set-version.ps1 {{VERSION}}

# --- Packaging (MSIS 3.x → MSI + multi-arch bundle) -------------------------

# Build the full release: x64 + x86 + ARM64 MSIs and the multi-arch bundle .exe, into setup\.
release: package-all

# Build all three per-architecture MSIs and the final multi-architecture bundle.
package-all: package-x64 package-x86 package-arm64 bundle

# Build the x64 MSI.
package-x64: (_stage "x64" "x64" "x64" "Release") (_package "pserv5-x64.msis")

# Build the x86 MSI (the x86 solution platform builds into bin\Win32\).
package-x86: (_stage "x86" "Win32" "x86" "Release") (_package "pserv5-x86.msis")

# Build the ARM64 MSI.
package-arm64: (_stage "ARM64" "ARM64" "arm64" "Release") (_package "pserv5-arm64.msis")

# Build the multi-architecture bundle .exe (requires the three MSIs built first).
bundle:
    msis /BUILD setup\setup-bundle.msis

# --- internal packaging machinery (hidden from `just --list`) ---------------

# Stage the Release payload (pserv5.exe + pservc.exe + themes\) for one arch.
# Args: <solution-platform> <bin-dir> <stage-name> <configuration>.
[private]
_stage msplatform bindir stagename configuration: (build-winui-release msplatform)
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\stage-release.ps1 {{msplatform}}

# Build one MSI from setup\ (standalone — the bundle adds prerequisites).
[private]
_package script:
    @cd setup&& msis /BUILD /STANDALONE {{script}}
