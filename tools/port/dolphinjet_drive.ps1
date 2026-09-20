# Drive an already-running DolphinJet: varied input plus periodic capture.
#
# Split out from dolphinjet_play_session.ps1 so a long session can be resumed
# rather than restarted, and so a wedged harness never costs the runtime it
# has already accumulated.
#
# It deliberately does NOT call AttachThreadInput. Doing that on every
# keypress, to steal focus back, blocked the harness solid on the first press
# while the game carried on rendering perfectly - a stuck measuring
# instrument reads exactly like a stuck subject. GetForegroundWindow is a
# cheap non-blocking call, so the target is checked and the press is dropped
# if the window is not the game's. Dropping a press is a lost input; typing it
# into whatever the user switched to is worse.
param(
    [string]$OutDir,
    [int]$RunSeconds = 600,
    [int]$ShotEverySeconds = 20,
    [int]$Seed = 20260920,
    [int]$ShotStart = 100
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DriveInput {
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
}
"@

$proc = Get-Process dolphinjet -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Host "[drive] no dolphinjet process"; exit 1 }
$hwnd = $proc.MainWindowHandle
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$KEYUP = 0x0002
$SCANCODE = 0x0008
# Set-1 scancodes, so the layout is irrelevant: a virtual-key code lands on
# the wrong physical key on AZERTY.
$keys = @{ A_BUTTON = 0x2C; UP = 0x11; LEFT = 0x1E; DOWN = 0x1F; RIGHT = 0x20; CAM_L = 0x24; CAM_R = 0x26 }
$dropped = 0

function Send-Key([int]$scan, [int]$holdMs) {
    if ([DriveInput]::GetForegroundWindow() -ne $script:hwnd) {
        $script:dropped++
        Start-Sleep -Milliseconds $holdMs
        return
    }
    [DriveInput]::keybd_event(0, $scan, $SCANCODE, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $holdMs
    [DriveInput]::keybd_event(0, $scan, $SCANCODE -bor $KEYUP, [UIntPtr]::Zero)
}

$rng = New-Object System.Random($Seed)
$moves = @($keys.UP, $keys.UP, $keys.UP, $keys.LEFT, $keys.RIGHT, $keys.DOWN, $keys.CAM_L, $keys.CAM_R)
$start = Get-Date
$deadline = $start.AddSeconds($RunSeconds)
$lastShot = $start
$shot = $ShotStart

while ((Get-Date) -lt $deadline) {
    if ($proc.HasExited) {
        Write-Host "[drive] CRASH apres $([int]((Get-Date) - $start).TotalSeconds) s (code $($proc.ExitCode))"
        exit 1
    }

    Send-Key $moves[$rng.Next(0, $moves.Length)] $rng.Next(250, 900)
    if ($rng.Next(0, 4) -eq 0) { Send-Key $keys.A_BUTTON $rng.Next(80, 250) }
    Start-Sleep -Milliseconds 150

    if (((Get-Date) - $lastShot).TotalSeconds -ge $ShotEverySeconds) {
        $lastShot = Get-Date
        $rect = New-Object DriveInput+RECT
        if ([DriveInput]::GetClientRect($hwnd, [ref]$rect)) {
            $w = $rect.Right - $rect.Left; $h = $rect.Bottom - $rect.Top
            if ($w -gt 0 -and $h -gt 0) {
                $bmp = New-Object System.Drawing.Bitmap($w, $h)
                $g = [System.Drawing.Graphics]::FromImage($bmp)
                $hdc = $g.GetHdc()
                [DriveInput]::PrintWindow($hwnd, $hdc, 2) | Out-Null
                $g.ReleaseHdc($hdc)
                $bmp.Save((Join-Path $OutDir ("play_{0:d3}.png" -f $shot)), [System.Drawing.Imaging.ImageFormat]::Png)
                $g.Dispose(); $bmp.Dispose()
            }
        }
        $shot++
        Write-Host ("[drive] {0,4}s  capture {1}  (drops {2})" -f [int]((Get-Date) - $start).TotalSeconds, $shot, $dropped)
    }
}
Write-Host "[drive] fini apres $RunSeconds s, $dropped appuis abandonnes faute de focus"
