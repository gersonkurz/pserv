<#
.SYNOPSIS
    Set the pserv6 version in the code global (pserv6\version.h) and every
    installer manifest (setup\*.msis). The .rc files read version.h, so the
    VERSIONINFO resources follow automatically. Invoked by:  just version X.Y.Z
#>
param([Parameter(Mandatory = $true)][string]$Version)

if ($Version -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
    Write-Error "Version must be X.Y.Z (e.g. 5.1.0); got '$Version'"
    exit 1
}
$major = $Matches[1]; $minor = $Matches[2]; $patch = $Matches[3]
$root = Split-Path -Parent $PSScriptRoot   # repo root (scripts\..)
$utf8 = New-Object System.Text.UTF8Encoding($false)

# 1) pserv6\version.h — the single source of truth for code and the .rc.
$verPath = Join-Path $root 'pserv6\version.h'
$h = Get-Content -Raw $verPath
$h = $h -replace '(#define\s+PSERV_VERSION_MAJOR\s+)\d+',  "`${1}$major"
$h = $h -replace '(#define\s+PSERV_VERSION_MINOR\s+)\d+',  "`${1}$minor"
$h = $h -replace '(#define\s+PSERV_VERSION_PATCH\s+)\d+',  "`${1}$patch"
$h = $h -replace '(#define\s+PSERV_VERSION_STRING\s+")[^"]*(")', "`${1}$Version`${2}"
$h = $h -replace '(#define\s+PSERV_VERSION_RC\s+)\d+,\d+,\d+,\d+', "`${1}$major,$minor,$patch,0"
[System.IO.File]::WriteAllText($verPath, $h, $utf8)

# 2) Installer manifests — patch PRODUCT_VERSION. BUILD_TARGET filenames and the
#    bundle's MSI references derive from it via {{PRODUCT_VERSION}}.
foreach ($name in @('pserv6-x64.msis', 'pserv6-arm64.msis', 'setup-bundle.msis')) {
    $msisPath = Join-Path $root (Join-Path 'setup' $name)
    if (-not (Test-Path $msisPath)) { continue }
    $msis = Get-Content -Raw $msisPath
    $msis = [regex]::Replace($msis, '(<set name="PRODUCT_VERSION" value=")[^"]*(")',
        { param($m) $m.Groups[1].Value + $Version + $m.Groups[2].Value })
    [System.IO.File]::WriteAllText($msisPath, $msis, $utf8)
}

Write-Host "pserv6 version set to $Version (pserv6\version.h + setup\*.msis)"
