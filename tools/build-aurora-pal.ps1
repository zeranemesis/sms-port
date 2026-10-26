param([int]$Jobs = [Math]::Min(8,[Environment]::ProcessorCount), [switch]$Archipelago)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($Jobs -lt 1) { throw 'Jobs doit etre superieur a zero.' }
$msysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { 'C:\msys64' }
$toolBin = Join-Path $msysRoot 'mingw64\bin'
$cmakeNative = Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'
$cmakeMingw = Join-Path $toolBin 'cmake.exe'
$python = Join-Path $toolBin 'python.exe'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
foreach ($file in @($cmakeNative,$cmakeMingw,$python,$vswhere,(Join-Path $toolBin 'g++.exe'))) {
    if (!(Test-Path -LiteralPath $file)) { throw "Outil absent : $file" }
}
$visualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$visualStudio) { throw 'Installer les outils C++ x64 de Visual Studio.' }
$vcvars = Join-Path $visualStudio 'VC\Auxiliary\Build\vcvars64.bat'
$bridge = Join-Path $projectRoot 'build\aurora-bridge'
$outputName = if ($Archipelago) { 'build\windows-64-pal-archipelago' } else { 'build\windows-64-pal-aurora' }
$output = Join-Path $projectRoot $outputName
$apOption = if ($Archipelago) { 'ON' } else { 'OFF' }
New-Item -ItemType Directory -Force -Path $bridge,$output | Out-Null
$nativePath = (($env:PATH -split ';') | Where-Object { $_ -and !$_.StartsWith($msysRoot,[StringComparison]::OrdinalIgnoreCase) }) -join ';'
# Isolate the MSVC environment from the MinGW phase (SDL2 and SDL3 remain
# in their respective runtimes). The dependency source cache is configurable
# through SMS_AURORA_DEPS in aurora-bridge's CMake cache.
$helper = Join-Path $bridge 'build-msvc.cmd'
$commands = @('@echo off','chcp 65001 >nul',"set `"PATH=$nativePath`"", "call `"$vcvars`"",'if errorlevel 1 exit /b %errorlevel%',
    "`"$cmakeNative`" -S `"$projectRoot\aurora-bridge`" -B `"$bridge`" -G Ninja -DCMAKE_MAKE_PROGRAM=`"$toolBin/ninja.exe`" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DAURORA_ENABLE_RMLUI=ON",
    'if errorlevel 1 exit /b %errorlevel%',
    "`"$cmakeNative`" --build `"$bridge`" --target sms_aurora --parallel $Jobs",'exit /b %errorlevel%')
[System.IO.File]::WriteAllLines($helper,$commands,(New-Object System.Text.UTF8Encoding($false)))
& $env:ComSpec /d /c $helper
if ($LASTEXITCODE -ne 0) { throw "Compilation Aurora echouee : $LASTEXITCODE" }
$env:PATH = "$toolBin;$(Join-Path $msysRoot 'usr\bin');$env:PATH"
& $cmakeMingw -S $projectRoot -B $output -G Ninja `
    "-DCMAKE_C_COMPILER=$toolBin/gcc.exe" "-DCMAKE_CXX_COMPILER=$toolBin/g++.exe" `
    "-DPython3_EXECUTABLE=$python" -DSMS_REGION=GMSP01 -DSMS_ARCH=64 `
    "-DSMS_ARCHIPELAGO=$apOption" -DSMS_GX_BACKEND=aurora -DSMS_GX_BUILD_TESTS=OFF -DSMS_WITH_GX=ON -DSMS_PARTYBOARD_UI=ON `
    "-DSMS_AURORA_BRIDGE_BUILD=$bridge" `
    "-DSMS_FREETYPE_SOURCE_DIR=$projectRoot/third_party/freetype" `
    "-DSMS_RMLUI_SOURCE_DIR=$projectRoot/third_party/rmlui" `
    "-DFETCHCONTENT_SOURCE_DIR_RCHEEVOS=$projectRoot/third_party/rcheevos"
if ($LASTEXITCODE -ne 0) { throw "Configuration du jeu echouee : $LASTEXITCODE" }
& $cmakeMingw --build $output --target sms --parallel $Jobs
if ($LASTEXITCODE -ne 0) { throw "Compilation du jeu echouee : $LASTEXITCODE" }
# A bridge-only change can leave sms.exe up to date: always refresh DLLs.
Get-ChildItem -LiteralPath $bridge -Filter '*.dll' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $output $_.Name)
}
if ($Archipelago) { Write-Host 'Compilation terminee. Lancer Jouer-Archipelago.cmd.' }
else { Write-Host 'Compilation terminee. Lancer Jouer-Aurora.cmd ; menu avec F1.' }
