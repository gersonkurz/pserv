$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Split-Path $PSScriptRoot -Parent)).Path
$header = Get-Content -Raw -LiteralPath (Join-Path $root 'pserv6/version.h')
$version = [regex]::Match($header, 'PSERV_VERSION_STRING\s+"([^"]+)"').Groups[1].Value
if (-not $version) { throw 'Cannot read release version.' }
$installer = New-Object -ComObject WindowsInstaller.Installer
$themes = @(Get-ChildItem -LiteralPath (Join-Path $root 'pserv6/themes') -Filter '*.yaml' | ForEach-Object Name)
foreach ($arch in @('x64', 'x86', 'ARM64')) {
    $stage = Join-Path $root ('dist/stage/' + $arch.ToLowerInvariant())
    $msi = Join-Path $root "setup/pserv$version $arch.msi"
    $db = $installer.OpenDatabase($msi, 0)
    $view = $db.OpenView('SELECT `FileName`, `FileSize` FROM `File`')
    $view.Execute()
    $files = @{}
    while ($record = $view.Fetch()) {
        $name = ($record.StringData(1) -split '\|')[-1]
        $files[$name] = $record.IntegerData(2)
    }
    $view.Close()
    foreach ($relative in @('pserv6.exe', 'pserv6c.exe', 'pserv6.pri', 'Microsoft.ui.xaml.dll', 'msvcp140.dll', 'vcruntime140.dll', 'docs/pserv6.md', 'docs/pserv6c.md', 'docs/release-review.md') + @($themes | ForEach-Object { 'themes/' + $_ })) {
        $file = Get-Item -LiteralPath (Join-Path $stage $relative)
        if (-not $files.ContainsKey($file.Name) -or $files[$file.Name] -ne $file.Length) { throw "$arch MSI missing/mismatched payload: $relative" }
    }
    foreach ($exe in @('pserv6.exe', 'pserv6c.exe')) {
        $reader = [IO.BinaryReader]::new([IO.File]::OpenRead((Join-Path $stage $exe)))
        try {
            $reader.BaseStream.Position = 0x3c
            $pe = $reader.ReadInt32()
            $reader.BaseStream.Position = $pe
            if ($reader.ReadUInt32() -ne 0x4550) { throw "Invalid PE: $exe" }
            $expected = @{x64=0x8664; x86=0x14c; ARM64=0xaa64}[$arch]
            if ($reader.ReadUInt16() -ne $expected) { throw "Wrong architecture: $arch/$exe" }
        } finally { $reader.Dispose() }
    }
    Write-Output "$arch MSI: matching GUI/CLI architecture, runtime files, manuals and $($themes.Count) themes verified."
}
$bundle = Join-Path $root "setup/pserv$version.exe"
$inspection = & msis /INSPECT $bundle
if ($LASTEXITCODE -ne 0) { throw 'Bundle inspection failed.' }
$inspection | Set-Content -LiteralPath (Join-Path $root 'dist/bundle-inspection.txt') -Encoding utf8
foreach ($arch in @('x64', 'x86', 'ARM64')) {
    $msi = Join-Path $root "setup/pserv$version $arch.msi"
    $hasher = [Security.Cryptography.SHA256]::Create()
    $inputStream = [IO.File]::OpenRead($msi)
    try { $digest = [BitConverter]::ToString($hasher.ComputeHash($inputStream)).Replace('-', '').Substring(0,16) }
    finally { $inputStream.Dispose(); $hasher.Dispose() }
    if (-not ($inspection -match [regex]::Escape($digest))) { throw "Bundle does not contain the current $arch MSI." }
}
Write-Output 'Bundle contains all three current MSIs.'
