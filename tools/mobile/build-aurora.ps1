param(
    [string]$Sdk = "$env:LOCALAPPDATA\Android\Sdk",
    [int]$Jobs = 2,
    [switch]$ConfigureOnly
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$cmake = 'C:\Program Files\CMake\bin\cmake.exe'
$ninja = Join-Path $Sdk 'cmake\3.22.1\bin\ninja.exe'
$toolchain = Join-Path $Sdk 'ndk\29.0.14206865\build\cmake\android.toolchain.cmake'
$output = Join-Path $repo 'build\android-arm64-aurora'
& $cmake -S (Join-Path $repo 'aurora-bridge') -B $output -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=RelWithDebInfo
if ($LASTEXITCODE -ne 0) { throw 'Android Aurora configuration failed' }
if (!$ConfigureOnly) {
    & $cmake --build $output --target sms_aurora --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Android Aurora build failed' }
}
