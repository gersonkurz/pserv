# Build automation for pserv6 — Windows system administration tool
# The GUI pserv6.exe (WinUI 3) and the CLI pservc.exe, both on the shared pserv_core library,
# plus pserv_core_tests. Platforms: ARM64 and x64.
# Requires: just, msbuild on PATH (VS 2026 Developer shell).
#
# Builds drive off the solution so $(SolutionDir) resolves correctly — the projects derive
# OutDir/IntDir from it. pserv6.exe (with its WinAppSDK runtime files) and pservc.exe land in
# pserv6\bin\<Platform>\<Config>\, which is the release payload; the tests in pserv6\bin\tests\.
# Packaging: `just release` (needs MSIS 3.x on PATH). Direction: docs/pserv6-plan.md.

set windows-shell := ["cmd.exe", "/c"]

solution := "pserv6\\pserv6.slnx"

# Native solution platform: ARM64 on ARM64 hardware, x64 otherwise. An x64 build of just itself
# runs emulated on ARM64 and sees PROCESSOR_ARCHITECTURE=AMD64, so PROCESSOR_IDENTIFIER
# ("ARMv8 ...") is checked as well.
_arch := env_var_or_default("PROCESSOR_ARCHITECTURE", "AMD64")
_cpu := env_var_or_default("PROCESSOR_IDENTIFIER", "")
platform := if _arch == "ARM64" { "ARM64" } else if _cpu =~ "^ARM" { "ARM64" } else { "x64" }
# Folder (under %VCToolsInstallDir%bin) holding the AddressSanitizer runtime DLLs for that platform.
_asan_bin := if platform == "ARM64" { "Hostarm64\\arm64" } else { "Hostx64\\x64" }

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

# Build everything: Debug+Release for x64 and ARM64
build-all: (_msbuild "Debug" "x64") (_msbuild "Release" "x64") (_msbuild "Debug" "ARM64") (_msbuild "Release" "ARM64")

# Build Debug (native arch), run pserv_core_tests, and check that every built pserv6.exe
# starts as the invoker (asInvoker; plan F6); exits non-zero if any check fails
test: build
    @pserv6\bin\tests\{{platform}}\Debug\pserv_core_tests.exe
    @powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check-manifest.ps1

# Build pserv_core + tests with AddressSanitizer (Debug, native arch, separate "-asan" output
# folders) and run the tests with the 50-round background-refresh stress run
test-asan: _require-devshell
    msbuild {{solution}} /restore /t:pserv_core_tests /p:Configuration=Debug /p:Platform={{platform}} /p:EnableASAN=true /p:PservBuildVariant=-asan /m /nologo /v:minimal
    @set "PATH=%VCToolsInstallDir%bin\{{_asan_bin}};%PATH%" && pserv6\bin\tests\{{platform}}\Debug-asan\pserv_core_tests.exe --stress 50

# Build Debug + launch pserv6 (native arch; it starts with the shell's rights)
run: build
    @pserv6\bin\{{platform}}\Debug\pserv6.exe

# Build Release (native arch) and run `pserv6 --selftest`: every view loaded, scrolled and
# refreshed, with timings against the plan's budgets. Elevated shell for the real figures;
# pserv6 runs with the shell's rights (`-RunAsInvoker` is accepted and no longer needed).
selftest *ARGS: build-release
    @powershell -NoProfile -ExecutionPolicy Bypass -File scripts\selftest.ps1 -Exe pserv6\bin\{{platform}}\Release\pserv6.exe {{ARGS}}

# Build Debug + launch the CLI (native arch); pass args after `--`, e.g. `just run-cli -- services`
run-cli *ARGS: build
    @pserv6\bin\{{platform}}\Debug\pservc.exe {{ARGS}}

# Clean + rebuild Debug (native arch)
rebuild: clean build

# Clean all build + packaging output (bin\, temp\, dist\)
clean:
    if exist pserv6\bin rmdir /s /q pserv6\bin
    if exist pserv6\temp rmdir /s /q pserv6\temp
    if exist dist rmdir /s /q dist

# Set the version (X.Y.Z) in version.h and every setup\*.msis (the .rc files read version.h).
version VERSION:
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\set-version.ps1 {{VERSION}}

# --- Packaging (MSIS 3.x → MSI + multi-arch bundle) -------------------------

# Build the full release: x64 + ARM64 MSIs and the multi-arch bundle .exe, into setup\.
release: package-all bundle

# Build both per-architecture MSIs.
package-all: package-x64 package-arm64

# Build the x64 MSI and its symbols zip.
package-x64: (_stage "x64" "x64") (_package "pserv6-x64.msis") (_symbols_zip "x64")

# Build the ARM64 MSI and its symbols zip.
package-arm64: (_stage "ARM64" "arm64") (_package "pserv6-arm64.msis") (_symbols_zip "arm64")

# Build the multi-architecture bundle .exe (requires both MSIs built first).
bundle:
    msis /BUILD setup\setup-bundle.msis

# Check an MSI without installing it: administrative extract into dist\extract\<arch>\,
# compared file by file with dist\stage\<arch>\ (the payload it was built from).
check-msi arch:
    @powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check-msi.ps1 -Arch {{arch}}

# --- internal packaging machinery (hidden from `just --list`) ---------------

# Stage one arch's Release payload: pserv6.exe, its WinAppSDK runtime files and pservc.exe into
# dist\stage\<arch>\ (no .pdb/.lib/.exp/.ilk/.winmd), the .pdb files into dist\symbols\<arch>\
# (zipped by _symbols_zip; not part of the MSI).
# Args: <solution-platform> <stage-name>.
[private]
_stage msplatform stagename: (_msbuild "Release" msplatform)
    @if exist dist\stage\{{stagename}} rmdir /s /q dist\stage\{{stagename}}
    @if exist dist\symbols\{{stagename}} rmdir /s /q dist\symbols\{{stagename}}
    @mkdir dist\stage\{{stagename}}
    @mkdir dist\symbols\{{stagename}}
    @robocopy pserv6\bin\{{msplatform}}\Release dist\stage\{{stagename}} /E /XF *.pdb *.exp *.lib *.ilk *.winmd >nul & if errorlevel 8 exit /b 1
    @copy /Y pserv6\bin\{{msplatform}}\Release\*.pdb dist\symbols\{{stagename}}\ >nul

# Build one MSI from setup\ (standalone — the bundle adds prerequisites).
[private]
_package script:
    @cd setup&& msis /BUILD /STANDALONE {{script}}

# Zip one arch's staged .pdb files next to the MSIs (they are not part of the MSI).
[private]
_symbols_zip stagename:
    @powershell -NoProfile -ExecutionPolicy Bypass -File scripts\zip-symbols.ps1 -Arch {{stagename}}
