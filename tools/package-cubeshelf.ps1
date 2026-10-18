# Builds the CubeShelf runtime package from build/windows-64-pal-aurora:
#   dist/cubeshelf-pal-<version>/sms-pal-win-x64.zip, checksums.txt, release-notes.md
# The archive holds sms.exe at its root (CubeShelf's RuntimeLaunchPaths), the
# DLLs beside it, res/partyboard, default settings and key bindings. No game
# disc or game files: CubeShelf extracts the player's own disc into GMSP01/files
# beside sms.exe and starts the game with SMS_DISC_ROOT pointing there.
#
# -Eclipse packages the Super Mario Eclipse build (build/windows-64-ecl, North
# American region, Eclipse's code built in) as sms-eclipse-win-x64.zip in the
# same folder: CubeShelf builds Eclipse's disc from the player's GMSE01 one into
# GMSE04/disc.iso and starts the game with SMS_DISC_IMAGE. checksums.txt then
# covers both archives, as one release carries both.
param(
    [Parameter(Mandatory = $true)][string]$Version,   # e.g. cubeshelf-pal-v0.2.0
    [string]$Build = '',
    [switch]$Eclipse
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$kind = if ($Eclipse) { 'eclipse' } else { 'pal' }
$asset = "sms-$kind-win-x64.zip"
if (-not $Build) { $Build = Join-Path $root $(if ($Eclipse) { 'build\windows-64-ecl' } else { 'build\windows-64-pal-aurora' }) }
$exe = Join-Path $Build 'sms.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "No build at $Build (run Construire-Aurora.cmd)" }
if (!(Test-Path -LiteralPath (Join-Path $Build 'sms_aurora.dll'))) { throw "$Build is not the Aurora build" }
$cache = Get-Content -LiteralPath (Join-Path $Build 'CMakeCache.txt') -ErrorAction SilentlyContinue
$builtEclipse = [bool]($cache -match '^SMS_ECLIPSE:BOOL=ON$')
if ($builtEclipse -ne [bool]$Eclipse) {
    throw $(if ($Eclipse) { "$Build is not built with SMS_ECLIPSE=ON" } else { "$Build is the Eclipse build (use -Eclipse)" })
}

$out = Join-Path $root "dist\$Version"
$package = Join-Path $out "package-$kind"
foreach ($old in @($package, (Join-Path $out $asset))) {
    if (Test-Path -LiteralPath $old) { Remove-Item -Recurse -Force -LiteralPath $old }
}
New-Item -ItemType Directory -Force -Path $package | Out-Null

# Never ship a game disc: tools/bundle_disc.py appends one behind an
# "SMSDISC1" trailer, and such an executable must stay on the player's PC.
$stream = [System.IO.File]::OpenRead($exe)
try {
    $tail = New-Object byte[] 32
    [void]$stream.Seek(-32, [System.IO.SeekOrigin]::End)
    [void]$stream.Read($tail, 0, 32)
} finally { $stream.Dispose() }
if ([System.Text.Encoding]::ASCII.GetString($tail, 0, 8) -eq 'SMSDISC1') { throw "$exe carries a bundled disc image" }
Copy-Item -LiteralPath $exe -Destination $package
# The build keeps DWARF debug info (about 200 MB); the package does not need it.
$msys = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { 'C:\msys64' }
$strip = Join-Path $msys 'mingw64\bin\strip.exe'
& $strip --strip-debug (Join-Path $package 'sms.exe')
if ($LASTEXITCODE -ne 0) { throw 'strip failed' }
Get-ChildItem -LiteralPath $Build -Filter '*.dll' -File | Copy-Item -Destination $package
Copy-Item -Recurse -LiteralPath (Join-Path $Build 'res') -Destination $package

# sms_aurora.dll is built against the shared MSVC runtime. Ship it beside the
# game (app-local deployment) so a PC without the VC++ redistributable starts.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$crt = Get-ChildItem -Path (Join-Path $vs 'VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT') -Directory |
    Sort-Object FullName -Descending | Select-Object -First 1
if (!$crt) { throw 'MSVC x64 redistributable folder not found' }
Get-ChildItem -LiteralPath $crt.FullName -Filter '*.dll' -File | Copy-Item -Destination $package

# Defaults from the repository, not the developer's own settings.txt. CubeShelf
# is the launcher, and it names the game data through SMS_DISC_ROOT.
$settings = (& git -C $root show HEAD:settings.txt) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'git show HEAD:settings.txt failed' }
$settings = [regex]::Replace($settings, '(?m)^launcher\s*=.*\n?', '').TrimEnd() + "`nlauncher = off"
$settings = [regex]::Replace($settings, '(?m)^(SMS_DISC_ROOT|SMS_DISC_IMAGE|disc_image)\s*=.*\n?', '')
[System.IO.File]::WriteAllText((Join-Path $package 'settings.txt'), $settings + "`n", (New-Object System.Text.UTF8Encoding($false)))
Copy-Item -LiteralPath (Join-Path $root 'bindings.txt') -Destination $package

$commit = (& git -C $root rev-parse --short HEAD).Trim()
$buildInfo = @{ region = $(if ($Eclipse) { 'GMSE01' } else { 'GMSP01' }); eclipse = [bool]$Eclipse
    architecture = 'x64'; renderer = 'aurora'; commit = $commit; version = $Version }
[System.IO.File]::WriteAllText((Join-Path $package 'sms-build.json'), ($buildInfo | ConvertTo-Json -Compress),
    (New-Object System.Text.UTF8Encoding($false)))
if ($Eclipse) {
@'
Super Mario Eclipse on the native Super Mario Sunshine runtime, for CubeShelf (Aurora renderer).
Eclipse's code (Super Mario Eclipse, BetterSunshineEngine, BetterSunshineMoveset) is built in.
No game disc or game files are included: CubeShelf builds Eclipse's disc from your own complete
North American disc (GMSE01) with the patch of the "Super Mario Eclipse" GameBanana mod.
F1 opens graphics, audio and controls.
'@ | Set-Content -Encoding utf8 -LiteralPath (Join-Path $package 'README.txt')
} else {
@'
Super Mario Sunshine native PAL runtime for CubeShelf (Aurora renderer: Direct3D 12, Vulkan or Direct3D 11).
No game disc or game files are included: CubeShelf prepares them from your own GMSP01 disc.
F1 opens graphics, audio and controls. Mods are managed by CubeShelf: file mods from GameBanana
are applied over the disc each launch. Super Mario Eclipse runs on its own runtime (sms-eclipse).
'@ | Set-Content -Encoding utf8 -LiteralPath (Join-Path $package 'README.txt')
}

$zip = Join-Path $out $asset
Compress-Archive -Path (Join-Path $package '*') -DestinationPath $zip -CompressionLevel Optimal
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant()
# One checksums.txt for every runtime archive in the folder (one release carries them all).
$sums = Get-ChildItem -LiteralPath $out -Filter 'sms-*-win-x64.zip' -File | Sort-Object Name | ForEach-Object {
    "$((Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant())  $($_.Name)" }
[System.IO.File]::WriteAllText((Join-Path $out 'checksums.txt'), (($sums -join "`n") + "`n"),
    (New-Object System.Text.UTF8Encoding($false)))
if ($Eclipse) {
@"
CubeShelf Super Mario Eclipse runtime - Windows x64

Built from zeranemesis/sms-port commit $commit (native-pal-menu) with SMS_ECLIPSE=ON, Aurora renderer.

Package: $asset
SHA-256: $hash

No game disc, extracted game files, Eclipse disc or patch is included. CubeShelf checks the
player's complete North American disc (GMSE01, MD5 0c6d2edae9fdf40dfc410ff1623e4119), applies
the xdelta patch of the installed "Super Mario Eclipse" GameBanana mod, checks the result
(MD5 caa546309e0443f7b47632b040a250d7) and starts the game with SMS_DISC_IMAGE=GMSE04/disc.iso.
"@ | Set-Content -Encoding utf8 -LiteralPath (Join-Path $out 'release-notes-eclipse.md')
Write-Host "Package: $zip ($([math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 1)) MB) sha256 $hash"
return
}
@"
CubeShelf Sunshine PAL runtime - Windows x64

Built from zeranemesis/sms-port commit $commit (native-pal-menu), Aurora renderer.

Package: $asset
SHA-256: $hash

No game disc, extracted game files or copyrighted game assets are included. CubeShelf
extracts the player's own European GMSP01 disc (ISO, GCM or RVZ) into GMSP01/files and
starts the game with SMS_DISC_ROOT, SMS_CUBESHELF_MOD_LIST and SMS_CUBESHELF=1.
The MSVC runtime DLLs are included, so the VC++ redistributable is not required.
"@ | Set-Content -Encoding utf8 -LiteralPath (Join-Path $out 'release-notes.md')
Write-Host "Package: $zip ($([math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 1)) MB) sha256 $hash"
