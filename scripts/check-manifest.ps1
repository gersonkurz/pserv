# Fails unless every built pserv6.exe embeds a manifest with requireAdministrator
# (docs/pserv6-plan.md F6: with ApplicationType=Windows Store, a missing EnableUAC silently drops
# UACExecutionLevel, and the app would start unelevated). Needs mt.exe (VS Developer shell).
param([string]$BinRoot = "pserv5\bin\pserv6")

$ErrorActionPreference = "Stop"
$exes = @(Get-ChildItem -Path $BinRoot -Recurse -Filter pserv6.exe -ErrorAction SilentlyContinue)
if ($exes.Count -eq 0) {
    Write-Host "manifest check: no pserv6.exe below $BinRoot"
    exit 1
}
$failed = 0
foreach ($exe in $exes) {
    $out = Join-Path ([IO.Path]::GetTempPath()) ("pserv6-manifest-" + [guid]::NewGuid() + ".xml")
    & mt.exe -nologo "-inputresource:$($exe.FullName);#1" "-out:$out" | Out-Null
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $out)) {
        Write-Host "manifest check: FAIL $($exe.FullName): no embedded manifest (mt.exe exit $LASTEXITCODE)"
        $failed++
        continue
    }
    $manifest = Get-Content $out -Raw
    Remove-Item $out
    if ($manifest -match 'requestedExecutionLevel\s+level="requireAdministrator"') {
        Write-Host "manifest check: ok   $($exe.FullName)"
    } else {
        Write-Host "manifest check: FAIL $($exe.FullName): requestedExecutionLevel is not requireAdministrator"
        $failed++
    }
}
exit $(if ($failed -eq 0) { 0 } else { 1 })
