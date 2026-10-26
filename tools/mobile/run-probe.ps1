param(
    [string]$Sdk = "$env:LOCALAPPDATA\Android\Sdk",
    [string]$Serial = '',
    [ValidateSet('arm64-v8a','x86_64')][string]$Abi = 'arm64-v8a'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildName = if ($Abi -eq 'arm64-v8a') { 'android-arm64-probe' } else { 'android-x86_64-probe' }
$binary = Join-Path $repo "build\$buildName\sms_mobile_probe"
$adb = Join-Path $Sdk 'platform-tools\adb.exe'
if (!(Test-Path -LiteralPath $binary)) { throw 'Run build-probe.ps1 first' }
$listing = & $adb devices
if ($LASTEXITCODE -ne 0) { throw 'Cannot query Android devices' }
$devices = @($listing | Where-Object { $_ -match '^\S+\s+device$' } | ForEach-Object { ($_ -split '\s+')[0] })
if ($Serial) {
    if ($Serial -notin $devices) { throw 'The selected device is not connected and authorized' }
} elseif ($devices.Count -eq 1) {
    $Serial = $devices[0]
} else {
    throw 'Connect one authorized Android device, or select it with -Serial'
}
$deviceAbis = & $adb -s $Serial shell getprop ro.product.cpu.abilist
if ($LASTEXITCODE -ne 0 -or $deviceAbis -notmatch "(^|,)$([regex]::Escape($Abi))(,|$)") {
    throw "The selected device does not support $Abi"
}
$hash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
$remote = "/data/local/tmp/sms_mobile_probe_$($hash.Substring(0,12))"
& $adb -s $Serial push $binary $remote
if ($LASTEXITCODE -ne 0) { throw 'Cannot transfer diagnostic' }
& $adb -s $Serial shell chmod 700 $remote
if ($LASTEXITCODE -ne 0) { throw 'Cannot mark diagnostic executable' }
$lines = & $adb -s $Serial shell $remote
$result = $LASTEXITCODE
$lines | Write-Output
$folder = Join-Path $repo 'artifacts\android-arm64-probe'
New-Item -ItemType Directory -Path $folder -Force | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
@{ sha256=$hash; abi=$Abi; exit_code=$result; output=@($lines); context='adb shell diagnostic, not Android Activity' } |
    ConvertTo-Json -Depth 4 | Out-File -LiteralPath (Join-Path $folder "run-$stamp.json") -Encoding utf8
if ($result -ne 0) { throw "Android diagnostic failed with exit code $result" }
