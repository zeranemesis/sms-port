param(
    [string]$Sdk = "$env:LOCALAPPDATA\Android\Sdk",
    [string]$NdkVersion = '29.0.14206865',
    [string]$CmakeVersion = '3.22.1',
    [ValidateSet('arm64-v8a','x86_64')][string]$Abi = 'arm64-v8a'
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ndk = Join-Path $Sdk "ndk\$NdkVersion"
$cmake = Join-Path $Sdk "cmake\$CmakeVersion\bin\cmake.exe"
$ninja = Join-Path $Sdk "cmake\$CmakeVersion\bin\ninja.exe"
$buildName = if ($Abi -eq 'arm64-v8a') { 'android-arm64-probe' } else { 'android-x86_64-probe' }
$output = Join-Path $repo "build\$buildName"
foreach ($required in @($cmake, $ninja, "$ndk\build\cmake\android.toolchain.cmake")) {
    if (!(Test-Path -LiteralPath $required)) { throw "Missing tool: $required" }
}
& $cmake -S $PSScriptRoot -B $output -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$ndk\build\cmake\android.toolchain.cmake" "-DANDROID_ABI=$Abi" -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=RelWithDebInfo
if ($LASTEXITCODE -ne 0) { throw 'Android diagnostic configuration failed' }
& $cmake --build $output
if ($LASTEXITCODE -ne 0) { throw 'Android diagnostic build failed' }
Write-Output "Built diagnostic (not the game): $output\sms_mobile_probe"
