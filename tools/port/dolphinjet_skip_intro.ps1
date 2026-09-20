# Launches dolphinjet, skips the intro cinematic by pressing A (mapped to the
# Z key by install_keyboard_fallback in src/port/recomp_pad.cpp), then lets the
# run continue so a profile can be taken of actual gameplay rather than of the
# THP decoder.
#
# Uses keybd_event rather than SendKeys: SDL reads real input events, and
# SendKeys posts WM_CHAR messages that an SDL application does not see.
param(
    [string]$Exe = "C:\Users\valen\sms-port\build-msvc\Release\dolphinjet.exe",
    [string]$OutDir,
    [int]$SkipAfterSeconds = 25,
    [int]$SkipPresses = 12,
    [int]$RunSeconds = 200,
    [int]$ShotEvery = 0,
    # Presses A again every N seconds during the run. The intro skip only gets
    # as far as the file-select screen, which waits for input like every other
    # menu; without this the harness sits there for the rest of the run.
    [int]$NudgeEvery = 0
)

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32Input {
    [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, IntPtr pid);
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint idAttach, uint idAttachTo, bool fAttach);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    // Windows refuses SetForegroundWindow from a process that is not already
    // in the foreground. Attaching to the current foreground window's input
    // queue first is the documented way round it, and focus is not optional
    // here: Aurora reads the keyboard through SDL_GetKeyboardState, which is
    // only fed by window messages.
    public static bool ForceForeground(IntPtr hWnd) {
        IntPtr fg = GetForegroundWindow();
        uint fgThread = GetWindowThreadProcessId(fg, IntPtr.Zero);
        uint thisThread = GetCurrentThreadId();
        AttachThreadInput(thisThread, fgThread, true);
        ShowWindow(hWnd, 9); // SW_RESTORE
        BringWindowToTop(hWnd);
        bool ok = SetForegroundWindow(hWnd);
        AttachThreadInput(thisThread, fgThread, false);
        return ok && GetForegroundWindow() == hWnd;
    }
}
"@

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$exeDir = Split-Path -Parent $Exe
Push-Location $exeDir
$proc = Start-Process -FilePath $Exe -PassThru `
    -RedirectStandardOutput (Join-Path $OutDir "skip_out.log") `
    -RedirectStandardError (Join-Path $OutDir "skip_err.log")
Pop-Location

Write-Host "[skip] demarre, attente de $SkipAfterSeconds s avant d'envoyer A"
Start-Sleep -Seconds $SkipAfterSeconds

if ($proc.HasExited) {
    Write-Host "[skip] CRASH avant meme le saut (code $($proc.ExitCode))"
    exit 1
}

# Bring the game window forward so the keystrokes reach it.
$proc.Refresh()
if ($proc.MainWindowHandle -ne [IntPtr]::Zero) {
    $focused = [Win32Input]::ForceForeground($proc.MainWindowHandle)
    Start-Sleep -Milliseconds 700
    if ($focused) {
        Write-Host "[skip] fenetre au premier plan : oui"
    } else {
        Write-Host "[skip] ATTENTION : la fenetre n'a PAS pu etre mise au premier plan - les touches n'arriveront pas, et un resultat negatif ne prouvera rien"
    }
} else {
    Write-Host "[skip] pas de fenetre principale trouvee - les touches n'arriveront pas"
}

# Send the PHYSICAL key, not the logical one.
#
# install_keyboard_fallback binds SDL_SCANCODE_Z, and an SDL scancode is a
# physical key position. keybd_event with a virtual-key code is logical: on this
# machine's French (AZERTY) layout Windows maps VK_Z to the key that types "z",
# which sits where QWERTY has W - so SDL would see SDL_SCANCODE_W and the
# binding would never match. Sending the set-1 scancode with KEYEVENTF_SCANCODE
# names the physical key directly and makes the layout irrelevant.
#
# 0x2C is the set-1 scancode for the QWERTY Z position, i.e. SDL_SCANCODE_Z.
$SCAN_Z = 0x2C
$KEYEVENTF_KEYUP = 0x0002
$KEYEVENTF_SCANCODE = 0x0008
for ($i = 0; $i -lt $SkipPresses; $i++) {
    [Win32Input]::keybd_event(0, $SCAN_Z, $KEYEVENTF_SCANCODE, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 150
    [Win32Input]::keybd_event(0, $SCAN_Z, $KEYEVENTF_SCANCODE -bor $KEYEVENTF_KEYUP, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 400
}
Write-Host "[skip] $SkipPresses appuis sur A envoyes"

$deadline = (Get-Date).AddSeconds($RunSeconds)
$shot = 0
$tick = 0
# Capture the game window ONLY, never the whole screen.
#
# A full-screen grab photographs whatever else the machine is showing, which is
# the developer's private desktop - it has already picked up unrelated content
# that had no business in a build log. The window rectangle is the only part
# this harness has any reason to look at.
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 10
    if ($proc.HasExited) {
        Write-Host "[skip] CRASH pendant le run (code $($proc.ExitCode))"
        exit 1
    }
    $tick++
    if ($NudgeEvery -gt 0 -and ($tick % $NudgeEvery) -eq 0) {
        # Re-focus first: anything the user clicks steals it, and a keystroke
        # sent to another window proves nothing.
        $proc.Refresh()
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero) {
            [Win32Input]::ForceForeground($proc.MainWindowHandle) | Out-Null
        }
        [Win32Input]::keybd_event(0, $SCAN_Z, $KEYEVENTF_SCANCODE, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 150
        [Win32Input]::keybd_event(0, $SCAN_Z, $KEYEVENTF_SCANCODE -bor $KEYEVENTF_KEYUP, [UIntPtr]::Zero)
    }
    if ($ShotEvery -gt 0 -and ($shot % $ShotEvery) -eq 0) {
        $proc.Refresh()
        $rect = New-Object Win32Input+RECT
        if ($proc.MainWindowHandle -ne [IntPtr]::Zero -and [Win32Input]::GetWindowRect($proc.MainWindowHandle, [ref]$rect)) {
            $w = $rect.Right - $rect.Left
            $h = $rect.Bottom - $rect.Top
            if ($w -gt 0 -and $h -gt 0) {
                $bmp = New-Object System.Drawing.Bitmap($w, $h)
                $g = [System.Drawing.Graphics]::FromImage($bmp)
                $g.CopyFromScreen((New-Object System.Drawing.Point($rect.Left, $rect.Top)), [System.Drawing.Point]::Empty, (New-Object System.Drawing.Size($w, $h)))
                $bmp.Save((Join-Path $OutDir ("skip_{0:d2}.png" -f $shot)), [System.Drawing.Imaging.ImageFormat]::Png)
                $g.Dispose(); $bmp.Dispose()
            }
        }
    }
    $shot++
}

if (-not $proc.HasExited) {
    $proc.CloseMainWindow() | Out-Null
    Start-Sleep -Seconds 2
    if (-not $proc.HasExited) { $proc.Kill() }
    Write-Host "[skip] PAS DE CRASH sur $RunSeconds s apres le saut"
}
