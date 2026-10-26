$launcherCode = @"
using System;
using System.Runtime.InteropServices;

public class DesktopLauncher {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct STARTUPINFO {
        public int cb;
        public string lpReserved;
        public string lpDesktop;
        public string lpTitle;
        public int dwX;
        public int dwY;
        public int dwXSize;
        public int dwYSize;
        public int dwXCountChars;
        public int dwYCountChars;
        public int dwFillAttribute;
        public int dwFlags;
        public short wShowWindow;
        public short cbReserved2;
        public IntPtr lpReserved2;
        public IntPtr hStdInput;
        public IntPtr hStdOutput;
        public IntPtr hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PROCESS_INFORMATION {
        public IntPtr hProcess;
        public IntPtr hThread;
        public int dwProcessId;
        public int dwThreadId;
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern bool CreateProcess(
        string lpApplicationName,
        string lpCommandLine,
        IntPtr lpProcessAttributes,
        IntPtr lpThreadAttributes,
        bool bInheritHandles,
        uint dwCreationFlags,
        IntPtr lpEnvironment,
        string lpCurrentDirectory,
        ref STARTUPINFO lpStartupInfo,
        out PROCESS_INFORMATION lpProcessInformation
    );

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr hObject);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    public static int LaunchVisible(string cmdLine, string cwd, out IntPtr processHandle) {
        STARTUPINFO si = new STARTUPINFO();
        si.cb = Marshal.SizeOf(si);
        si.lpDesktop = @"WinSta0\Default";
        PROCESS_INFORMATION pi;
        bool ok = CreateProcess(null, cmdLine, IntPtr.Zero, IntPtr.Zero, false, 0x00000010 /* CREATE_NEW_CONSOLE */, IntPtr.Zero, cwd, ref si, out pi);
        if (!ok) {
            processHandle = IntPtr.Zero;
            return -Marshal.GetLastWin32Error();
        }
        CloseHandle(pi.hThread);
        processHandle = pi.hProcess;
        return pi.dwProcessId;
    }
}
"@

Add-Type -TypeDefinition $launcherCode -ErrorAction SilentlyContinue

$outPng = "C:\Users\BEAVSN\.gemini\antigravity\brain\135109a1-e25f-4b17-beba-bcb8d82b3ca3\desktop_test.png"
if (Test-Path $outPng) { Remove-Item $outPng -Force }

$cmd = "powershell.exe -ExecutionPolicy Bypass -File C:\sms-unified\tools\capture_screen.ps1 `"$outPng`""
[IntPtr]$hProc = [IntPtr]::Zero
$pid = [DesktopLauncher]::LaunchVisible($cmd, "C:\sms-unified", [ref]$hProc)
Write-Host "Launched capture on WinSta0\Default with PID $pid"
[DesktopLauncher]::WaitForSingleObject($hProc, 5000)
[DesktopLauncher]::CloseHandle($hProc)

if (Test-Path $outPng) {
    Write-Host "SUCCESS: Screenshot captured! Size: $((Get-Item $outPng).Length) bytes"
} else {
    Write-Host "FAILED: Screenshot not generated"
}
