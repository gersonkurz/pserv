$ErrorActionPreference = 'Stop'
if ($env:VisualStudioVersion -ne '18.0' -or -not (Get-Command cl.exe -ErrorAction SilentlyContinue))
{
    throw 'Run in a Visual Studio 2026 Developer shell.'
}
$projectRoot = Split-Path $PSScriptRoot -Parent
$testOutput = Join-Path $projectRoot 'dist\service-edit-checks'
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
$arguments = @('/nologo', '/std:c++latest', '/EHsc', '/MD', '/utf-8', '/DPSERV_WINUI_BUILD', '/DSPDLOG_USE_STD_FORMAT')
foreach ($include in @('pserv6', 'spdlog\include', 'wil\include', 'rapidjson\include', 'tomlplusplus\include'))
{
    $arguments += '/I' + (Join-Path $projectRoot $include)
}
foreach ($source in @('tests\service_edit_checks.cpp', 'pserv6\controllers\services_data_controller.cpp',
    'pserv6\models\service_info.cpp', 'pserv6\core\data_controller.cpp', 'pserv6\core\data_object_container.cpp'))
{
    $arguments += Join-Path $projectRoot $source
}
$arguments += @('/Fe:service-edit-checks.exe', '/link', 'advapi32.lib', 'user32.lib')
Push-Location $testOutput
try
{
    & cl.exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "Check compilation failed: $LASTEXITCODE" }
    & .\service-edit-checks.exe
    if ($LASTEXITCODE -ne 0) { throw "Service editing checks failed: $LASTEXITCODE" }
}
finally { Pop-Location }
