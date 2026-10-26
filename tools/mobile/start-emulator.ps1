param(
    [string]$Sdk = "$env:LOCALAPPDATA\Android\Sdk",
    [ValidateSet('host','swiftshader','lavapipe')][string]$Gpu = 'swiftshader',
    [switch]$Visible
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$emulator = Join-Path $Sdk 'emulator\emulator.exe'
$adb = Join-Path $Sdk 'platform-tools\adb.exe'
$avd = 'Sunshine_Android_Test'
$available = & $emulator -list-avds
if ($LASTEXITCODE -ne 0 -or $avd -notin $available) {
    throw 'Create the Sunshine_Android_Test AVD first; see tools/mobile/README.md'
}
$devices = & $adb devices
if ($devices -match '^emulator-5580\s') {
    Write-Output 'The Sunshine emulator is already running on emulator-5580'
    exit 0
}
$folder = Join-Path $repo 'artifacts\android-arm64-probe'
New-Item -ItemType Directory -Path $folder -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$style = if ($Visible) { 'Normal' } else { 'Hidden' }
$p = Start-Process -FilePath $emulator -ArgumentList @('-avd',$avd,'-port','5580','-gpu',$Gpu,'-memory','4096','-cores','4','-no-snapshot','-no-boot-anim') -WindowStyle $style -RedirectStandardOutput (Join-Path $folder "emulator-$stamp.stdout.log") -RedirectStandardError (Join-Path $folder "emulator-$stamp.stderr.log") -PassThru
Write-Output "Started Android x86_64 emulator, launcher PID $($p.Id). ARM64 validation remains separate."
