$ErrorActionPreference = 'Stop'
if ($env:VisualStudioVersion -ne '18.0') { throw 'Use a VS 2026 Developer shell.' }
$root = Split-Path $PSScriptRoot -Parent
$output = Join-Path $root 'dist/environment-edit-checks'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    & cl.exe /nologo /std:c++latest /EHsc /MD /utf-8 ('/I' + (Join-Path $root 'pserv5')) (Join-Path $root 'tests/environment_edit_checks.cpp') /Fe:environment-edit-checks.exe
    if ($LASTEXITCODE) { throw 'Compilation failed' }
    & ./environment-edit-checks.exe
    if ($LASTEXITCODE) { throw 'Checks failed' }
} finally { Pop-Location }
