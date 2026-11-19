param([Parameter(Mandatory=$true)][ValidateSet('x64','x86','ARM64')][string]$Platform)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Split-Path $PSScriptRoot -Parent)).Path
$binPlatform = if ($Platform -eq 'x86') { 'Win32' } else { $Platform }
$arch = $Platform.ToLowerInvariant()
$output = Join-Path $root "pserv5/bin/$binPlatform/Release"
$gui = Join-Path $output 'pserv6'
$stage = Join-Path $root "dist/stage/$arch"
$symbols = Join-Path $root "dist/symbols/$arch"
foreach ($target in @($stage, $symbols)) {
    $absolute = [IO.Path]::GetFullPath($target)
    if (-not $absolute.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw "Outside workspace: $absolute" }
    if (Test-Path -LiteralPath $absolute) { Remove-Item -LiteralPath $absolute -Recurse -Force }
    New-Item -ItemType Directory -Path $absolute -Force | Out-Null
}
foreach ($required in @((Join-Path $gui 'pserv6.exe'), (Join-Path $gui 'pserv6.pri'), (Join-Path $output 'pservc.exe'))) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing build output: $required" }
}
& robocopy $gui $stage /E /XF *.pdb *.exp *.lib *.ilk /NFL /NDL /NJH /NJS /NP
if ($LASTEXITCODE -ge 8) { throw "Staging failed: $LASTEXITCODE" }
Copy-Item -LiteralPath (Join-Path $output 'pservc.exe') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'docs/pservc.md') -Destination (Join-Path $stage 'docs')
foreach ($file in @((Join-Path $gui 'pserv6.pdb'), (Join-Path $output 'pservc.pdb'))) {
    if (Test-Path -LiteralPath $file) { Copy-Item -LiteralPath $file -Destination $symbols }
}
Write-Output "Staged pserv6 and CLI with the self-contained Windows App SDK runtime for $Platform."
exit 0
