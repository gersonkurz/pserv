# Zips one architecture's staged .pdb files (dist\symbols\<arch>\) into
# setup\pserv6-<version>-<arch>-symbols.zip. The symbols are not part of the MSI (plan section 7:
# MSI size budget); they ship next to it.
param([Parameter(Mandatory = $true)][ValidateSet("x64", "arm64")][string]$Arch)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$version = [regex]::Match((Get-Content -Raw (Join-Path $root "pserv6\version.h")), '#define\s+PSERV_VERSION_STRING\s+"([^"]+)"').Groups[1].Value
$zip = Join-Path $root "setup\pserv6-$version-$Arch-symbols.zip"
Compress-Archive -Force -Path (Join-Path $root "dist\symbols\$Arch\*") -DestinationPath $zip
Write-Host ("symbols: {0} ({1:N1} MB)" -f (Split-Path -Leaf $zip), ((Get-Item $zip).Length / 1MB))
