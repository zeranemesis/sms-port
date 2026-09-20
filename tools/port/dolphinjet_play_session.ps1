# Long scripted play session: varied input, periodic capture, defect hunting.
#
# This is SCRIPTED input, not a human playing. It cannot tell you whether the
# game feels right, whether the controls respond the way they should, or
# whether something looks wrong - a person found the frame flicker in seconds
# that hours of counters here had missed. What it can do is drive the game for
# long enough, with enough variety, to surface crashes, hangs, log floods and
# states the intro-skip harness never reaches.
#
# Keys follow install_keyboard_fallback (src/port/recomp_pad.cpp): Z is the A
# button, X is B, W/A/S/D are the left stick, I/J/K/L the C stick. START is
# deliberately never sent - it pauses the game and would end the session
# without that being visible in a log.
param(
    [string]$Exe = "C:\Users\valen\sms-port\build-msvc\Release\dolphinjet.exe",
    [string]$OutDir,
    [int]$BootSeconds = 22,
    [int]$RunSeconds = 720,
    [int]$ShotEverySeconds = 20,
    [int]$Seed = 20260920
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class PlayInput {
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, IntPtr pid);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    public static bool Focus(IntPtr hWnd) {
        IntPtr fg = GetForegroundWindow();
        uint fgThread = GetWindowThreadProcessId(fg, IntPtr.Zero);
        uint me = GetCurrentThreadId();
        AttachThreadInput(me, fgThread, true);
        BringWindowToTop(hWnd);
        bool ok = SetForegroundWindow(hWnd);
        AttachThreadInput(me, fgThread, false);
        return ok;
    }
}
"@

$KEYUP = 0x0002
$SCANCODE = 0x0008
# Set-1 scancodes, so the keyboard layout is irrelevant - a virtual-key code
# lands on the wrong physical key on AZERTY.
$keys = @{
    A_BUTTON = 0x2C  # Z
    B_BUTTON = 0x2D  # X
    UP       = 0x11  # W
    LEFT     = 0x1E  # A
    DOWN     = 0x1F  # S
    RIGHT    = 0x20  # D
    CAM_L    = 0x24  # J
    CAM_R    = 0x26  # L
}

$script:GameWindow = [IntPtr]::Zero
$script:LostFocus = 0

# keybd_event goes to whatever window has focus. If the user clicks away mid
# session, every movement key would be typed into whatever they switched to.
# So the target is checked before each press and the press is dropped rather
# than sent somewhere it does not belong.
#
# The check is GetForegroundWindow alone. Refocusing here, through Focus()'s
# AttachThreadInput, is not worth the risk: attaching to another process's
# input queue on every keypress can block, and a harness that blocks reads
# exactly like a game that has frozen.
function Send-Key([int]$scan, [int]$holdMs) {
    if ([PlayInput]::GetForegroundWindow() -ne $script:GameWindow) {
        $script:LostFocus++
        Start-Sleep -Milliseconds $holdMs
        return
    }
    [PlayInput]::keybd_event(0, $scan, $SCANCODE, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $holdMs
    [PlayInput]::keybd_event(0, $scan, $SCANCODE -bor $KEYUP, [UIntPtr]::Zero)
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Push-Location (Split-Path -Parent $Exe)
$proc = Start-Process -FilePath $Exe -PassThru `
    -RedirectStandardOutput (Join-Path $OutDir "play_out.log") `
    -RedirectStandardError (Join-Path $OutDir "play_err.log")
Pop-Location

Start-Sleep -Seconds $BootSeconds
if ($proc.HasExited) { Write-Host "[play] CRASH avant le debut (code $($proc.ExitCode))"; exit 1 }
$script:GameWindow = $proc.MainWindowHandle
[PlayInput]::Focus($script:GameWindow) | Out-Null
Start-Sleep -Milliseconds 600

# Get through the title and file select.
for ($i = 0; $i -lt 8; $i++) { Send-Key $keys.A_BUTTON 120; Start-Sleep -Milliseconds 500 }

$rng = New-Object System.Random($Seed)
$moves = @($keys.UP, $keys.UP, $keys.UP, $keys.LEFT, $keys.RIGHT, $keys.DOWN, $keys.CAM_L, $keys.CAM_R)
$deadline = (Get-Date).AddSeconds($RunSeconds)
$lastShot = Get-Date
$shot = 0
$start = Get-Date

while ((Get-Date) -lt $deadline) {
    if ($proc.HasExited) {
        $elapsed = [int]((Get-Date) - $start).TotalSeconds
        Write-Host "[play] CRASH apres $elapsed s de jeu (code $($proc.ExitCode))"
        exit 1
    }

    # A move, sometimes a jump. Held for a realistic fraction of a second
    # rather than tapped, so Mario actually travels.
    Send-Key $moves[$rng.Next(0, $moves.Length)] $rng.Next(250, 900)
    if ($rng.Next(0, 4) -eq 0) { Send-Key $keys.A_BUTTON $rng.Next(80, 250) }
    Start-Sleep -Milliseconds 150

    if (((Get-Date) - $lastShot).TotalSeconds -ge $ShotEverySeconds) {
        $lastShot = Get-Date
        $rect = New-Object PlayInput+RECT
        if ([PlayInput]::GetClientRect($proc.MainWindowHandle, [ref]$rect)) {
            $w = $rect.Right - $rect.Left; $h = $rect.Bottom - $rect.Top
            if ($w -gt 0 -and $h -gt 0) {
                $bmp = New-Object System.Drawing.Bitmap($w, $h)
                $g = [System.Drawing.Graphics]::FromImage($bmp)
                $hdc = $g.GetHdc()
                [PlayInput]::PrintWindow($proc.MainWindowHandle, $hdc, 2) | Out-Null
                $g.ReleaseHdc($hdc)
                $bmp.Save((Join-Path $OutDir ("play_{0:d3}.png" -f $shot)), [System.Drawing.Imaging.ImageFormat]::Png)
                $g.Dispose(); $bmp.Dispose()
            }
        }
        $shot++
        Write-Host ("[play] {0,4}s  capture {1}" -f [int]((Get-Date) - $start).TotalSeconds, $shot)
    }
}

if (-not $proc.HasExited) {
    $proc.CloseMainWindow() | Out-Null
    Start-Sleep -Seconds 2
    if (-not $proc.HasExited) { $proc.Kill() }
    Write-Host "[play] PAS DE CRASH sur $RunSeconds s de jeu scripte"
}
if ($script:LostFocus -gt 0) {
    Write-Host "[play] $($script:LostFocus) appuis abandonnes: la fenetre n'avait pas le focus"
}
