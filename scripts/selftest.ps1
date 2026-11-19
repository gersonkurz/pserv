# Runs `pserv6 --selftest` and prints its timing log. Exits 0 only if the log ends in
# RESULT: PASS *and* pserv6 exited with code 0: the PASS line is written before the window closes,
# so a crash during shutdown shows up only in the exit code.
# pserv6 requires administrator: run from an elevated shell, or pass -RunAsInvoker to measure
# unelevated (some views then show less data).
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [int]$Seconds = 3,
    [string]$Views = "",   # comma-separated view names, e.g. "Services,Scheduled Tasks"
    [switch]$RunAsInvoker
)

$ErrorActionPreference = "Stop"
$log = Join-Path ([IO.Path]::GetTempPath()) "pserv6-selftest.log"
Remove-Item $log -ErrorAction SilentlyContinue
if ($RunAsInvoker) { $env:__COMPAT_LAYER = "RunAsInvoker" }
# Start-Process joins the list into one command line, so every value is quoted here (view names
# contain spaces).
$arguments = @("--selftest")
if ($Views) { $arguments += "`"$Views`"" }
$arguments += @("--seconds", $Seconds, "--log", "`"$log`"")
$p = Start-Process -FilePath $Exe -ArgumentList $arguments -PassThru
$handle = $p.Handle # keep the handle open, or ExitCode is unavailable after exit
if (-not $p.WaitForExit(600000)) {
    $p.Kill()
    Write-Host "selftest: pserv6 did not exit within 10 minutes"
    exit 1
}
$exitCode = $p.ExitCode
if (-not (Test-Path $log)) {
    Write-Host "selftest: no log written (exit code $exitCode)"
    exit 1
}
Get-Content $log
$last = Get-Content $log -Tail 1
Write-Host "selftest: exit code $exitCode"
if ($last -ne "RESULT: PASS") { exit 1 }
if ($exitCode -ne 0) {
    Write-Host "selftest: FAIL - the log says PASS, but pserv6 exited with code $exitCode"
    exit 1
}
exit 0
