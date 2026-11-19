# Compile and run the pserv6 C++ regression test suite against the shared core & controller sources.
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$outDir = Join-Path $repoRoot 'pserv6\temp\tests'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$exePath = Join-Path $outDir 'pserv_tests.exe'

$sources = @(
    'tests\pserv_tests.cpp',
    'pserv6\core\data_object_container.cpp',
    'pserv6\core\data_controller.cpp',
    'pserv6\core\data_action.cpp',
    'pserv6\core\data_action_dispatch_context.cpp',
    'pserv6\core\async_operation.cpp',
    'pserv6\core\exporters\exporter_registry.cpp',
    'pserv6\models\service_info.cpp',
    'pserv6\windows_api\service_manager.cpp',
    'pserv6\actions\service_actions.cpp',
    'pserv6\controllers\services_data_controller.cpp',
    'pserv6\models\environment_variable_info.cpp',
    'pserv6\windows_api\environment_variable_manager.cpp',
    'pserv6\actions\environment_variable_actions.cpp',
    'pserv6\controllers\environment_variables_data_controller.cpp',
    'pserv6\theme\base16_theme.cpp',
    'pserv6\config\section.cpp',
    'pserv6\config\settings.cpp',
    'pserv6\utils\logging.cpp'
) | ForEach-Object { Join-Path $repoRoot $_ }

$includes = @(
    'pserv6',
    'spdlog\include',
    'tomlplusplus\include',
    'rapidjson\include',
    'wil\include',
    'fkYAML\include',
    'argparse\include'
) | ForEach-Object { "/I$(Join-Path $repoRoot $_)" }

$clArgs = @(
    '/nologo',
    '/std:c++latest',
    '/EHsc',
    '/utf-8',
    '/W3',
    '/MP4',
    '/D_CONSOLE',
    '/DSPDLOG_USE_STD_FORMAT',
    "/Fo$outDir\",
    "/Fe$exePath"
) + $includes + $sources + @(
    '/link',
    'advapi32.lib',
    'user32.lib',
    'shell32.lib',
    'ole32.lib',
    'oleaut32.lib',
    'uuid.lib',
    'shlwapi.lib'
)

Write-Host "Building pserv_tests.exe..." -ForegroundColor Cyan
& cl.exe @clArgs
if ($LASTEXITCODE -ne 0) {
    throw "Test compilation failed with exit code $LASTEXITCODE"
}

Write-Host "Running pserv_tests.exe..." -ForegroundColor Cyan
& $exePath
if ($LASTEXITCODE -ne 0) {
    throw "Tests failed with exit code $LASTEXITCODE"
}
