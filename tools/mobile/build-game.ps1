param(
    [string]$Sdk = "$env:LOCALAPPDATA\Android\Sdk",
    [string]$NdkVersion = '29.0.14206865',
    [int]$Jobs = 2,
    [ValidateSet('sms_game','sms','sms_mobile_surface_probe','sms_gx_static_integration_test')]
    [string]$Target = 'sms_game',
    [switch]$App
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
$ninja = Join-Path $Sdk 'cmake\3.22.1\bin\ninja.exe'
$toolchain = Join-Path $Sdk "ndk\$NdkVersion\build\cmake\android.toolchain.cmake"
$deps = Join-Path $repo 'build\windows-64-pal-aurora\_deps\rcheevos-src'
$output = Join-Path $repo 'build\android-arm64-game'
if (!(Test-Path -LiteralPath (Join-Path $deps 'include\rc_client.h'))) {
    throw 'Local rcheevos sources are missing; prepare the dependency sources first'
}
if ($Jobs -lt 1) { throw 'Jobs must be positive' }
$env:PATH = 'C:\msys64\mingw64\bin;C:\msys64\usr\bin;' + $env:PATH
$appMode = if ($App) { 'ON' } else { 'OFF' }
& $cmake -S $repo -B $output -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DSMS_ANDROID_APP=$appMode" -DSMS_ARCH=64 -DSMS_WITH_GX=ON -DSMS_GX_BACKEND=aurora -DSMS_PARTYBOARD_UI=ON -DSMS_BUNDLE_DISC=OFF "-DFETCHCONTENT_SOURCE_DIR_RCHEEVOS=$deps"
if ($LASTEXITCODE -ne 0) { throw 'Android game configuration failed' }
& $cmake --build $output --target $Target --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Android target $Target failed; inspect the first compiler/linker error" }
Write-Output "Android target $Target built in $output. This does not validate gameplay or package a game APK."
