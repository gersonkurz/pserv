$ErrorActionPreference = 'Stop'
# IsWow64Process2 reports the native machine even under ARM64 x64 emulation.
# RuntimeInformation.OSArchitecture on .NET Framework can report X64 there.
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NativePlatform
{
    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsWow64Process2(IntPtr process, out ushort processMachine, out ushort nativeMachine);
}
'@
[uint16]$processMachine = 0
[uint16]$nativeMachine = 0
if (-not [NativePlatform]::IsWow64Process2([IntPtr](-1), [ref]$processMachine, [ref]$nativeMachine))
{
    throw [System.ComponentModel.Win32Exception]::new([Runtime.InteropServices.Marshal]::GetLastWin32Error())
}
switch ($nativeMachine)
{
    0xAA64 { 'ARM64' }
    0x8664 { 'x64' }
    0x014c { 'x86' }
    default { throw 'Unsupported Windows architecture.' }
}
