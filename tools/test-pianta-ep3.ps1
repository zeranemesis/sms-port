param(
    [string]$Warp = "8,2",
    [string]$Label = "pianta_ep3_test",
    [int]$WaitSeconds = 14
)

$ErrorActionPreference = 'Stop'
$projectRoot = "C:\sms-unified"
$exe = "C:\sms-unified\build\windows-64-pal-aurora\sms.exe"
$outDir = "C:\Users\BEAVSN\.gemini\antigravity\brain\135109a1-e25f-4b17-beba-bcb8d82b3ca3"

$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
$env:SMS_SETTINGS = "$projectRoot\settings.txt"
$env:SMS_BINDINGS = "$projectRoot\bindings.txt"
$env:SMS_DISC_IMAGE = $null
$env:SMS_DISC_ROOT = "$projectRoot\rom\GMSP01-extracted\files"
$env:SMS_SAVE_DIR = "$projectRoot\saves\card-a"
$env:SMS_FRONTEND_DIR = "$projectRoot\saves\frontend"
$env:SMS_WARP = $Warp
$env:SMS_FRAME_RATE = "60"
$env:SMS_SKIP_MOVIES = "1"
$env:SMS_AUTOPRESS = "START@30,START@60,A@90,A@120,A@150,A@180,A@210,A@240"
$env:SMS_GX_COPY_LOG = "1"

. "$projectRoot\tools\DesktopLauncher.ps1"

Get-Process -Name "sms" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

[IntPtr]$hProc = [IntPtr]::Zero
$gamePid = [DesktopLauncher]::LaunchVisible("`"$exe`" --no-launcher", $projectRoot, [ref]$hProc)
Write-Host "Launched $Label (Warp $Warp) with PID $gamePid..."

Start-Sleep -Seconds $WaitSeconds

$screenPng = "$outDir\${Label}.png"
$captureCmd = "powershell.exe -ExecutionPolicy Bypass -File C:\sms-unified\tools\capture_screen.ps1 `"$screenPng`""
[IntPtr]$hCap = [IntPtr]::Zero
[DesktopLauncher]::LaunchVisible($captureCmd, $projectRoot, [ref]$hCap)
[DesktopLauncher]::WaitForSingleObject($hCap, 5000)
[DesktopLauncher]::CloseHandle($hCap)

Start-Sleep -Seconds 1

Stop-Process -Id $gamePid -Force -ErrorAction SilentlyContinue
[DesktopLauncher]::CloseHandle($hProc)

Write-Host "Finished test for $Label. Screenshot: $screenPng"
