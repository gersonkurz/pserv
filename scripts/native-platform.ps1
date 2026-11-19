# Detect true native host CPU architecture via kernel32!IsWow64Process2,
# returning "ARM64" on ARM64 Windows and "x64" otherwise (even when invoked from an x64 binary under emulation).
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @"
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

public static class NativeHostArch
{
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool IsWow64Process2(IntPtr hProcess, out ushort pProcessMachine, out ushort pNativeMachine);

    public static string GetPlatform()
    {
        try
        {
            ushort processMachine = 0;
            ushort nativeMachine = 0;
            if (IsWow64Process2(Process.GetCurrentProcess().Handle, out processMachine, out nativeMachine))
            {
                if (nativeMachine == 0xAA64) // IMAGE_FILE_MACHINE_ARM64
                {
                    return "ARM64";
                }
                return "x64";
            }
        }
        catch
        {
        }

        string envArch = Environment.GetEnvironmentVariable("PROCESSOR_ARCHITEW6432");
        if (string.IsNullOrEmpty(envArch))
        {
            envArch = Environment.GetEnvironmentVariable("PROCESSOR_ARCHITECTURE");
        }
        return (envArch != null && envArch.Equals("ARM64", StringComparison.OrdinalIgnoreCase)) ? "ARM64" : "x64";
    }
}
"@

[NativeHostArch]::GetPlatform()
