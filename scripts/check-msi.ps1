# Checks a built pserv6 MSI without installing it (installing would upgrade an installed pserv5
# away: plan D12). An administrative extract (msiexec /a) writes the MSI's files into
# dist\extract\<arch>\; they are compared, by relative path, size and SHA-256, with what the MSI was built
# from (dist\stage\<arch>\; the symbols are not in the MSI). With -SelfTest the extracted pserv6.exe also
# runs its self-test (unelevated, RunAsInvoker), which shows the payload launches as installed.
param(
    [Parameter(Mandatory = $true)][ValidateSet("x64", "arm64")][string]$Arch,
    [switch]$SelfTest
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$version = [regex]::Match((Get-Content -Raw (Join-Path $root "pserv6\version.h")), '#define\s+PSERV_VERSION_STRING\s+"([^"]+)"').Groups[1].Value
$msi = Join-Path $root "setup\pserv6-$version-$Arch.msi"
if (-not (Test-Path $msi)) { Write-Host "check-msi: $msi not found (just package-$Arch)"; exit 1 }

$extract = Join-Path $root "dist\extract\$Arch"
if (Test-Path $extract) { Remove-Item $extract -Recurse -Force }
New-Item -ItemType Directory -Force $extract | Out-Null
$log = Join-Path $root "dist\extract\msiexec-$Arch.log"
$p = Start-Process msiexec.exe -ArgumentList @("/a", "`"$msi`"", "/qn", "TARGETDIR=`"$extract`"", "/l*v", "`"$log`"") -Wait -PassThru
if ($p.ExitCode -ne 0) { Write-Host "check-msi: msiexec /a failed with $($p.ExitCode) (log: $log)"; exit 1 }

$exe = Get-ChildItem $extract -Recurse -Filter pserv6.exe | Select-Object -First 1
if (-not $exe) { Write-Host "check-msi: no pserv6.exe in the extract"; exit 1 }
$installed = $exe.DirectoryName

function Get-Listing($dir) {
    $base = (Resolve-Path $dir).Path.TrimEnd('\') + '\'
    $map = @{}
    # Length and SHA-256: same-sized but different files must not pass
    foreach ($f in Get-ChildItem $dir -Recurse -File) { $map[$f.FullName.Substring($base.Length).ToLowerInvariant()] = "{0}:{1}" -f $f.Length, (Get-FileHash $f.FullName -Algorithm SHA256).Hash }
    return $map
}
$expected = Get-Listing (Join-Path $root "dist\stage\$Arch")
$actual = Get-Listing $installed

$missing = @($expected.Keys | Where-Object { -not $actual.ContainsKey($_) })
$extra = @($actual.Keys | Where-Object { -not $expected.ContainsKey($_) })
$different = @($expected.Keys | Where-Object { $actual.ContainsKey($_) -and $actual[$_] -ne $expected[$_] })
$msiSize = (Get-Item $msi).Length / 1MB
Write-Host ("check-msi {0}: {1} ({2:N1} MB); extracted {3} files to {4}" -f $Arch, (Split-Path -Leaf $msi), $msiSize, $actual.Count, $installed)
Write-Host ("check-msi {0}: expected {1} files; missing {2}, extra {3}, different content {4}" -f $Arch, $expected.Count, $missing.Count, $extra.Count, $different.Count)
foreach ($m in ($missing | Select-Object -First 10)) { Write-Host "  missing: $m" }
foreach ($m in ($extra | Select-Object -First 10)) { Write-Host "  extra:   $m" }
foreach ($m in ($different | Select-Object -First 10)) { Write-Host "  content: $m" }
$ok = $missing.Count -eq 0 -and $extra.Count -eq 0 -and $different.Count -eq 0
foreach ($name in "pserv6.exe", "pservc.exe") {
    if (-not $actual.ContainsKey($name)) { Write-Host "check-msi: $name is not in the payload"; $ok = $false }
}

if ($SelfTest) {
    & (Join-Path $PSScriptRoot "selftest.ps1") -Exe $exe.FullName -Seconds 1 -RunAsInvoker
    if ($LASTEXITCODE -ne 0) { $ok = $false }
}
Write-Host ("check-msi {0}: {1}" -f $Arch, $(if ($ok) { "PASS" } else { "FAIL" }))
exit $(if ($ok) { 0 } else { 1 })
