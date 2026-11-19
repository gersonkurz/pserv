param([ValidateSet('ARM64', 'x64', 'x86')][string]$Platform = 'ARM64')
$ErrorActionPreference = 'Stop'
if ($env:VisualStudioVersion -ne '18.0' -or $env:VSCMD_ARG_TGT_ARCH -ine $Platform)
{
    throw "Use a Visual Studio 2026 Developer shell targeting $Platform."
}
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildPlatform = if ($Platform -eq 'x86') { 'Win32' } else { $Platform }
$library = Join-Path $projectRoot "pserv6\bin\$buildPlatform\Release\pserv6-core.lib"
if (-not (Test-Path -LiteralPath $library)) { throw 'Build Release before running this check.' }
$testOutput = Join-Path $projectRoot 'dist\exporter-link-checks'
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
$includes = @('pserv6', 'spdlog/include', 'wil/include', 'rapidjson/include', 'tomlplusplus/include') | ForEach-Object { '/I' + (Join-Path $projectRoot $_) }
Push-Location $testOutput
try
{
    & cl.exe /nologo /std:c++latest /EHsc /MD /utf-8 /DNDEBUG /DSPDLOG_USE_STD_FORMAT @includes (Join-Path $projectRoot 'tests\exporter_link_checks.cpp') /Fe:exporter-link-checks.exe /link /LTCG $library
    if ($LASTEXITCODE -ne 0) { throw "Exporter link compilation failed: $LASTEXITCODE" }
    & .\exporter-link-checks.exe
    if ($LASTEXITCODE -ne 0) { throw "Exporter link checks failed: $LASTEXITCODE" }
}
finally { Pop-Location }
