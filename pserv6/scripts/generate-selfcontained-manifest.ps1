param(
    [Parameter(Mandatory)] [string] $AppxManifest,
    [Parameter(Mandatory)] [string] $MsixContentDir,
    [Parameter(Mandatory)] [string] $OutputManifest
)

$ErrorActionPreference = 'Stop'

$header = @"
<?xml version='1.0' encoding='utf-8' standalone='yes'?>
<assembly manifestVersion='1.0'
    xmlns:asmv3='urn:schemas-microsoft-com:asm.v3'
    xmlns:winrtv1='urn:schemas-microsoft-com:winrt.v1'
    xmlns='urn:schemas-microsoft-com:asm.v1'>
"@

$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine($header)

[xml]$doc = Get-Content -Path $AppxManifest
$ns = New-Object Xml.XmlNamespaceManager($doc.NameTable)
$ns.AddNamespace('m', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')

$allDlls = @{}
Get-ChildItem -Path $MsixContentDir -Filter '*.dll' | ForEach-Object { $allDlls[$_.Name] = $true }

foreach ($server in $doc.SelectNodes('//m:Package/m:Extensions/m:Extension/m:InProcessServer', $ns)) {
    $dllName = $server.SelectSingleNode('m:Path', $ns).InnerText
    $allDlls.Remove($dllName)

    [void]$sb.AppendLine("    <asmv3:file name='$dllName'>")
    foreach ($cls in $server.SelectNodes('m:ActivatableClass', $ns)) {
        $className = $cls.GetAttribute('ActivatableClassId')
        [void]$sb.AppendLine("        <winrtv1:activatableClass name='$className' threadingModel='both'/>")
    }
    [void]$sb.AppendLine('    </asmv3:file>')
}

[void]$sb.AppendLine('</assembly>')

$dir = Split-Path -Parent $OutputManifest
if ($dir -and !(Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
}

[System.IO.File]::WriteAllText($OutputManifest, $sb.ToString(), [System.Text.Encoding]::UTF8)
Write-Host "Generated manifest: $OutputManifest"

