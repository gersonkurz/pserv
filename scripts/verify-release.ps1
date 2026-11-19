# Verify staged Release payloads, PE machine headers, bundled Microsoft.VC145.CRT DLLs,
# per-architecture MSI File tables, and the multi-arch bundle executable.
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$versionHeader = Get-Content (Join-Path $repoRoot 'pserv6\version.h') -Raw
if ($versionHeader -notmatch '#define\s+PSERV_VERSION_STRING\s+"([^"]+)"') {
    throw "Could not parse PSERV_VERSION_STRING from pserv6\version.h"
}
$version = $Matches[1]
Write-Host "=== Verifying pserv6 v$version release artifacts ===" -ForegroundColor Cyan

function Get-PeMachine([string]$filePath) {
    $bytes = [System.IO.File]::ReadAllBytes($filePath)
    if ($bytes.Length -lt 0x40 -or $bytes[0] -ne 0x4D -or $bytes[1] -ne 0x5A) {
        throw "Not a valid MZ/PE binary: $filePath"
    }
    $peOffset = [System.BitConverter]::ToInt32($bytes, 0x3C)
    if ($peOffset -lt 0 -or ($peOffset + 6) -gt $bytes.Length) {
        throw "Invalid PE header offset in: $filePath"
    }
    if ($bytes[$peOffset] -ne 0x50 -or $bytes[$peOffset + 1] -ne 0x45 -or $bytes[$peOffset + 2] -ne 0 -or $bytes[$peOffset + 3] -ne 0) {
        throw "Missing PE signature in: $filePath"
    }
    return [System.BitConverter]::ToUInt16($bytes, $peOffset + 4)
}

function Get-Sha256Hex([string]$filePath) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $stream = [System.IO.File]::OpenRead($filePath)
        try {
            $hashBytes = $sha.ComputeHash($stream)
            return ([System.BitConverter]::ToString($hashBytes)).Replace('-', '')
        } finally {
            $stream.Dispose()
        }
    } finally {
        $sha.Dispose()
    }
}

function Get-MsiFileNames([string]$msiPath) {
    $installer = New-Object -ComObject WindowsInstaller.Installer
    $db = $installer.GetType().InvokeMember('OpenDatabase', 'InvokeMethod', $null, $installer, @($msiPath, 0))
    $view = $db.GetType().InvokeMember('OpenView', 'InvokeMethod', $null, $db, @('SELECT FileName FROM File'))
    $null = $view.GetType().InvokeMember('Execute', 'InvokeMethod', $null, $view, $null)
    $names = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    while ($true) {
        $rec = $view.GetType().InvokeMember('Fetch', 'InvokeMethod', $null, $view, $null)
        if ($null -eq $rec) { break }
        $rawName = $rec.GetType().InvokeMember('StringData', 'GetProperty', $null, $rec, @(1))
        $parts = $rawName -split '\|'
        $longName = if ($parts.Length -gt 1) { $parts[1] } else { $parts[0] }
        [void]$names.Add($longName)
    }
    $null = $view.GetType().InvokeMember('Close', 'InvokeMethod', $null, $view, $null)
    [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($view)
    [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($db)
    [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer)
    return $names
}

$expectedThemeCount = (Get-ChildItem (Join-Path $repoRoot 'pserv6\themes\*.yaml')).Count
$targets = @(
    @{ Name = 'x64';   Stage = 'x64';   Msi = "pserv$version x64.msi";   Machine = 0x8664 },
    @{ Name = 'x86';   Stage = 'x86';   Msi = "pserv$version x86.msi";   Machine = 0x014C },
    @{ Name = 'ARM64'; Stage = 'arm64'; Msi = "pserv$version ARM64.msi"; Machine = 0xAA64 }
)

$requiredPayloadFiles = @(
    'pserv6.exe',
    'pservc.exe',
    'pserv6.pri',
    'Microsoft.ui.xaml.dll',
    'msvcp140.dll',
    'vcruntime140.dll'
)

foreach ($t in $targets) {
    $stageDir = Join-Path $repoRoot "dist\stage\$($t.Stage)"
    if (-not (Test-Path $stageDir)) {
        throw "Missing staged directory: $stageDir"
    }

    foreach ($req in $requiredPayloadFiles) {
        $fullPath = Join-Path $stageDir $req
        if (-not (Test-Path $fullPath)) {
            throw "[$($t.Name)] Missing required staged file: $req"
        }
        if ($req -match '\.(exe|dll)$') {
            $actualMachine = Get-PeMachine $fullPath
            if ($actualMachine -ne $t.Machine) {
                throw ("[{0}] PE architecture mismatch for {1}: expected 0x{2:X4}, got 0x{3:X4}" -f $t.Name, $req, $t.Machine, $actualMachine)
            }
        }
    }

    $stagedThemes = (Get-ChildItem (Join-Path $stageDir 'themes\*.yaml') -ErrorAction SilentlyContinue).Count
    if ($stagedThemes -ne $expectedThemeCount) {
        throw "[$($t.Name)] Theme count mismatch: expected $expectedThemeCount, found $stagedThemes"
    }

    $msiPath = Join-Path $repoRoot "setup\$($t.Msi)"
    if (-not (Test-Path $msiPath)) {
        throw "[$($t.Name)] Missing MSI: $msiPath"
    }

    $msiFiles = Get-MsiFileNames $msiPath
    foreach ($req in $requiredPayloadFiles) {
        if (-not $msiFiles.Contains($req)) {
            throw "[$($t.Name)] MSI '$($t.Msi)' File table is missing: $req"
        }
    }

    $msiHash = Get-Sha256Hex $msiPath
    $msiSizeMb = [Math]::Round((Get-Item $msiPath).Length / 1MB, 2)
    Write-Host ("  [OK] {0,-5} MSI ({1,6:N2} MB, {2} files, {3} themes) SHA256={4}" -f $t.Name, $msiSizeMb, $msiFiles.Count, $stagedThemes, $msiHash.Substring(0, 16)) -ForegroundColor Green
}

$bundlePath = Join-Path $repoRoot "setup\pserv$version.exe"
if (-not (Test-Path $bundlePath)) {
    throw "Missing multi-arch bundle: $bundlePath"
}
$bundleMachine = Get-PeMachine $bundlePath
$bundleHash = Get-Sha256Hex $bundlePath
$bundleSizeMb = [Math]::Round((Get-Item $bundlePath).Length / 1MB, 2)
Write-Host ("  [OK] Bundle ({0,6:N2} MB, PE=0x{1:X4}) SHA256={2}" -f $bundleSizeMb, $bundleMachine, $bundleHash) -ForegroundColor Green
Write-Host "=== Release verification passed ===" -ForegroundColor Cyan
