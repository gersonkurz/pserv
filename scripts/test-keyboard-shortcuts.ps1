$ErrorActionPreference = 'Stop'
if ($env:VisualStudioVersion -ne '18.0' -or -not (Get-Command cl.exe -ErrorAction SilentlyContinue))
{
    throw 'Run in a Visual Studio 2026 Developer shell.'
}
$projectRoot = Split-Path $PSScriptRoot -Parent
$testOutput = Join-Path $projectRoot 'dist\keyboard-shortcut-checks'
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
Push-Location $testOutput
try
{
    & cl.exe /nologo /std:c++latest /EHsc /MD ('/I' + (Join-Path $projectRoot 'pserv6')) (Join-Path $projectRoot 'tests\keyboard_shortcut_checks.cpp') /Fe:keyboard-shortcut-checks.exe
    if ($LASTEXITCODE -ne 0) { throw "Check compilation failed: $LASTEXITCODE" }
    & .\keyboard-shortcut-checks.exe
    if ($LASTEXITCODE -ne 0) { throw "Keyboard routing checks failed: $LASTEXITCODE" }
}
finally { Pop-Location }
