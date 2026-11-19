param([string]$Platform = 'ARM64', [string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
if ($env:VisualStudioVersion -ne '18.0' -or -not (Get-Command cl.exe -ErrorAction SilentlyContinue))
{
    throw 'Run in a Visual Studio 2026 Developer shell.'
}
if ($env:VSCMD_ARG_TGT_ARCH -ine $Platform)
{
    throw "Use a Developer shell targeting $Platform, matching the generated WinUI headers."
}
$projectRoot = Split-Path $PSScriptRoot -Parent
$generated = Join-Path $projectRoot "pserv5\temp\pserv6\$Platform\$Configuration\Generated Files"
if (-not (Test-Path -LiteralPath (Join-Path $generated 'DataRow.g.h')))
{
    throw 'Build pserv6 for this platform and configuration first.'
}
$testOutput = Join-Path $projectRoot 'dist\winui-cell-collection-checks'
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
$includes = @($generated, 'pserv5/pserv6', 'pserv5', 'pserv5/pserv6/packages/Microsoft.WindowsAppSDK.WinUI.2.7.0/include', 'spdlog/include', 'wil/include', 'rapidjson/include', 'tomlplusplus/include') | ForEach-Object {
    if ([IO.Path]::IsPathRooted($_)) { '/I' + $_ } else { '/I' + (Join-Path $projectRoot $_) }
}
$sources = @('tests/winui_cell_collection_checks.cpp', 'pserv5/pserv6/DataRow.cpp', 'pserv5/pserv6/DataCell.cpp') | ForEach-Object { Join-Path $projectRoot $_ }
Push-Location $testOutput
try
{
    & cl.exe /nologo /std:c++latest /EHsc /MD /utf-8 /bigobj /MP /DPSERV_WINUI_BUILD /DSPDLOG_USE_STD_FORMAT /DNDEBUG @includes @sources /Fe:winui-cell-collection-checks.exe /link runtimeobject.lib ole32.lib oleaut32.lib
    if ($LASTEXITCODE -ne 0) { throw "Check compilation failed: $LASTEXITCODE" }
    & .\winui-cell-collection-checks.exe
    if ($LASTEXITCODE -ne 0) { throw "Cell collection checks failed: $LASTEXITCODE" }
}
finally { Pop-Location }
