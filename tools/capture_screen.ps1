param(
    [string]$OutputFile = "C:\Users\BEAVSN\.gemini\antigravity\brain\135109a1-e25f-4b17-beba-bcb8d82b3ca3\screen.png"
)

Add-Type @"
using System;
using System.Runtime.InteropServices;

public class WindowFocusHelper {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    public static readonly IntPtr HWND_TOP = new IntPtr(0);

    public static void FocusGame() {
        IntPtr hWnd = FindWindow(null, "Super Mario Sunshine");
        if (hWnd == IntPtr.Zero) {
            hWnd = FindWindow(null, "Aurora");
        }
        if (hWnd != IntPtr.Zero) {
            ShowWindow(hWnd, 9 /* SW_RESTORE */);
            SetWindowPos(hWnd, HWND_TOP, 0, 0, 1280, 720, 0x0040 /* SWP_SHOWWINDOW */);
            SetForegroundWindow(hWnd);
        }
    }
}
"@ -ErrorAction SilentlyContinue

[WindowFocusHelper]::FocusGame()
Start-Sleep -Milliseconds 300

Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
$bmp.Save($OutputFile, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Host "Screenshot saved to $OutputFile"
