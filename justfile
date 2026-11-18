# Build automation for pserv — Windows system administration tool
# GUI (pserv5.exe, Dear ImGui) and CLI (pservc.exe), both on the shared pserv_core library,
# plus pserv_core_tests. Requires: just, msbuild on PATH (VS 2026 Developer shell).
#
# Builds drive off the solution so $(SolutionDir) resolves correctly — the
# projects derive OutDir/IntDir from it. Output lands in pserv5\bin\<Platform>\<Config>\.
# Packaging: `just release` (needs MSIS 3.x on PATH). Direction: docs/pserv6-plan.md.

set windows-shell := ["cmd.exe", "/c"]

solution := "pserv5\\pserv5.slnx"

# Map %PROCESSOR_ARCHITECTURE% to the native solution platform: AMD64 → x64, ARM64 → ARM64.
_arch := env_var_or_default("PROCESSOR_ARCHITECTURE", "AMD64")
platform := if _arch == "ARM64" { "ARM64" } else { "x64" }

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
    msbuild {{solution}} /restore /p:Configuration={{configuration}} /p:Platform={{platform}} /m /nologo /v:minimal

# Build Debug (native arch) — every project in the solution
build: (_msbuild "Debug" platform)

# Build Release (native arch)
build-release: (_msbuild "Release" platform)

# Build everything: Debug+Release across x64, x86, ARM64
build-all: (_msbuild "Debug" "x64") (_msbuild "Release" "x64") (_msbuild "Debug" "x86") (_msbuild "Release" "x86") (_msbuild "Debug" "ARM64") (_msbuild "Release" "ARM64")

# Build Debug (native arch) and run pserv_core_tests; exits non-zero if any check fails
test: build
    @pserv5\bin\tests\{{platform}}\Debug\pserv_core_tests.exe

# Build Debug + launch the GUI (native arch)
run: build
    @pserv5\bin\{{platform}}\Debug\pserv5.exe

# Build Debug + launch the CLI (native arch); pass args after `--`, e.g. `just run-cli -- services`
run-cli *ARGS: build
    @pserv5\bin\{{platform}}\Debug\pservc.exe {{ARGS}}

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
release: package-all bundle

# Build all three per-architecture MSIs.
package-all: package-x64 package-x86 package-arm64

# Build the x64 MSI.
package-x64: (_stage "x64" "x64" "x64" "Release") (_stage_symbols "x64" "x64" "Release") (_package "pserv5-x64.msis")

# Build the x86 MSI (the x86 solution platform builds into bin\Win32\).
package-x86: (_stage "x86" "Win32" "x86" "Release") (_stage_symbols "Win32" "x86" "Release") (_package "pserv5-x86.msis")

# Build the ARM64 MSI.
package-arm64: (_stage "ARM64" "ARM64" "arm64" "Release") (_stage_symbols "ARM64" "arm64" "Release") (_package "pserv5-arm64.msis")

# Build the multi-architecture bundle .exe (requires the three MSIs built first).
bundle:
    msis /BUILD setup\setup-bundle.msis

# --- internal packaging machinery (hidden from `just --list`) ---------------

# Stage the Release payload (pserv5.exe + pservc.exe + themes\) for one arch.
# Args: <solution-platform> <bin-dir> <stage-name> <configuration>.
[private]
_stage msplatform bindir stagename configuration: (_msbuild configuration msplatform)
    @if exist dist\stage\{{stagename}} rmdir /s /q dist\stage\{{stagename}}
    @mkdir dist\stage\{{stagename}}
    @robocopy pserv5\bin\{{bindir}}\{{configuration}} dist\stage\{{stagename}} /E /XF *.pdb *.exp *.lib *.ilk >nul & if errorlevel 8 exit /b 1

# Stage the Release debug symbols for one arch (optional installer feature).
# Args: <bin-dir> <stage-name> <configuration>.
[private]
_stage_symbols bindir stagename configuration:
    @if exist dist\symbols\{{stagename}} rmdir /s /q dist\symbols\{{stagename}}
    @mkdir dist\symbols\{{stagename}}
    @copy /Y pserv5\bin\{{bindir}}\{{configuration}}\*.pdb dist\symbols\{{stagename}}\ >nul

# Build one MSI from setup\ (standalone — the bundle adds prerequisites).
[private]
_package script:
    @cd setup&& msis /BUILD /STANDALONE {{script}}
