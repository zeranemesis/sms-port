param([int]$Jobs = [Environment]::ProcessorCount)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$msysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { 'C:\msys64' }
$toolBin = Join-Path $msysRoot 'mingw64\bin'
$env:PATH = "$toolBin;$(Join-Path $msysRoot 'usr\bin');$env:PATH"
$cmakeExe = Join-Path $toolBin 'cmake.exe'
$pythonExe = Join-Path $toolBin 'python.exe'
foreach ($requiredFile in @($cmakeExe, $pythonExe, (Join-Path $toolBin 'g++.exe'))) {
    if (-not (Test-Path -LiteralPath $requiredFile)) { throw "Outil absent : $requiredFile" }
}
if ($Jobs -lt 1) { throw 'Jobs doit etre superieur a zero.' }
$outputRoot = Join-Path $projectRoot 'build\windows-64-pal-unified'
Push-Location $projectRoot
try {
    & $cmakeExe -S $projectRoot -B $outputRoot -G Ninja `
        "-DCMAKE_C_COMPILER=$toolBin/gcc.exe" "-DCMAKE_CXX_COMPILER=$toolBin/g++.exe" `
        "-DPython3_EXECUTABLE=$pythonExe" -DSMS_REGION=GMSP01 -DSMS_ARCH=64 `
        -DSMS_GX_BUILD_TESTS=OFF -DSMS_WITH_GX=ON -DSMS_PARTYBOARD_UI=ON `
        "-DSMS_FREETYPE_SOURCE_DIR=$projectRoot/third_party/freetype" `
        "-DSMS_RMLUI_SOURCE_DIR=$projectRoot/third_party/rmlui" `
        "-DFETCHCONTENT_SOURCE_DIR_RCHEEVOS=$projectRoot/third_party/rcheevos"
    if ($LASTEXITCODE -ne 0) { throw "Configuration CMake echouee : $LASTEXITCODE" }
    & $cmakeExe --build $outputRoot --target sms --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Compilation echouee : $LASTEXITCODE" }
    foreach ($dllName in @('SDL2.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
        $dllSource = Join-Path $toolBin $dllName
        if (-not (Test-Path -LiteralPath $dllSource)) { throw "DLL absente : $dllSource" }
        Copy-Item -LiteralPath $dllSource -Destination (Join-Path $outputRoot $dllName)
    }
    Write-Host 'Compilation terminee. Lancer Jouer.cmd.'
} finally {
    Pop-Location
}
