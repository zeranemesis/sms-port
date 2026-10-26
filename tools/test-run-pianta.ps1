param(
    [string]$Warp = "8,2",
    [string]$Label = "pianta_ep3_gameplay",
    [int]$WaitSeconds = 6
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
$env:SMS_AUTOPRESS = "START@60+10,START@120+10,START@180+10,START@240+10,START@300+10,START@360+10,STICK_LEFT@450+60,STICK_LEFT@520+60,A@600+15,A@660+15,A@720+15,A@780+15,STICK_UP@900+500,A@1000+20,R@1100+100"

. "$projectRoot\tools\DesktopLauncher.ps1"

Get-Process -Name "sms" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

[IntPtr]$hProc = [IntPtr]::Zero
$gamePid = [DesktopLauncher]::LaunchVisible("`"$exe`" --no-launcher", $projectRoot, [ref]$hProc)
Write-Host "Launched $Label (Warp $Warp) with PID $gamePid..."

Start-Sleep -Seconds $WaitSeconds

# Take screenshot with game brought to focus
$screenPng = "$outDir\${Label}.png"
$captureCmd = "powershell.exe -ExecutionPolicy Bypass -File C:\sms-unified\tools\capture_screen.ps1 `"$screenPng`""
[IntPtr]$hCap = [IntPtr]::Zero
[DesktopLauncher]::LaunchVisible($captureCmd, $projectRoot, [ref]$hCap)
[DesktopLauncher]::WaitForSingleObject($hCap, 5000)
[DesktopLauncher]::CloseHandle($hCap)

Start-Sleep -Seconds 2

Stop-Process -Id $gamePid -Force -ErrorAction SilentlyContinue
[DesktopLauncher]::CloseHandle($hProc)

Write-Host "Finished test for $Label. Screenshot: $screenPng"
