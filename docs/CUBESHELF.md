# CubeShelf

[CubeShelf](https://github.com/zeranemesis/CubeShelf-Launcher) installs this port, prepares the game files from the player's own disc and manages GameBanana mods.
This page is the contract between the two.

## Runtime package

`tools/package-cubeshelf.ps1 -Version cubeshelf-pal-vX.Y.Z` builds `dist/cubeshelf-pal-vX.Y.Z/` from `build/windows-64-pal-aurora`:
`sms-pal-win-x64.zip`, `checksums.txt` and `release-notes.md`.
The archive has `sms.exe` at its root, every DLL beside it (including the MSVC runtime, so the VC++ redistributable is not needed), `res/partyboard`, the repository's default `settings.txt` with `launcher = off`, `bindings.txt` and `sms-build.json`.
`sms.exe` is stripped of its debug information.
The script refuses an executable with a bundled disc (`tools/bundle_disc.py`): no game file is ever in the package.

CubeShelf takes the newest GitHub release of zeranemesis/sms-port and its `sms-pal-win-x64.zip`, checked against `checksums.txt`.

## Launch

An ISO or GCM is opened by the game as it is: CubeShelf extracts nothing and adds `SMS_DISC_IMAGE=<absolute path of the image>`, which the game takes over `SMS_DISC_ROOT`.
CubeShelf converts an RVZ with DolphinTool, extracts the disc's files into `GMSP01/files` beside `sms.exe`, and starts `sms.exe` in its own folder with:

```text
SMS_DISC_ROOT=GMSP01/files
SMS_CUBESHELF_MOD_LIST=<absolute path>/Mods/GMSP01/active-mods.txt
SMS_CUBESHELF=1
```

The extracted folder has no `sys/fst.bin`; the port builds the file table from the folder tree.
Saves stay in `%APPDATA%\sms-port\card-a`, outside the runtime folder, so a runtime update keeps them.

## Mod list

`active-mods.txt` is UTF-8 text, one mod content root per line, **highest priority first**.
Blank lines and lines starting with `#` are ignored, a UTF-8 BOM is tolerated, and a relative line is resolved against the list's folder.
A root that no longer exists is skipped with a warning; a list with no usable root boots the untouched disc.

A content root maps onto the **root of the disc**: CubeShelf has already chosen a mod's `files/` folder, so `…\files\data\scene\bianco0.szs` replaces `/data/scene/bianco0.szs`.
Roots are applied lowest priority first, so the highest priority mod wins a file it shares with another.
A mod may add files the disc does not have.
`cubeshelf-mod.json` is launcher metadata and never reaches the game.

A pack of loose files at its root says nothing about where they go.
A root file the disc does not have at its root, but has at exactly one other path, is placed at that path; the log says so (`loose airport0.szs placed at /data/scene/airport0.szs`).

Disc patches (`.xdelta`, `.vcdiff`, `.bps`, `.ips`) build a different disc and are not applied over this one; the log names them.
CubeShelf reports such a mod as unusable on Super Mario Sunshine's mod page.

## Super Mario Eclipse

Eclipse is a disc patch that also changes the game's code (see [ECLIPSE.md](ECLIPSE.md)), so it is a game of its own in CubeShelf (`GMSE04`), on its own runtime.
`tools/package-cubeshelf.ps1 -Version <same version> -Eclipse` packages `build/windows-64-ecl` (`-DSMS_REGION=GMSE01 -DSMS_ECLIPSE=ON`) as `sms-eclipse-win-x64.zip` beside the PAL archive, and rewrites `checksums.txt` for both: one release carries the two runtimes.

The player installs the "Super Mario Eclipse" mod (GameBanana 536309) from that game's Mods tab and picks a complete North American disc (`GMSE01`).
Its data preparation (`DiscPatch` in CubeShelf's catalogue) checks the disc's MD5 (`0c6d2edae9fdf40dfc410ff1623e4119`, the only image Eclipse's patch applies to) and refuses any other with a message naming the expected image: a trimmed, compressed or other-region copy cannot be patched.
It then applies the mod's `v1.1.0.xdelta` with CubeShelf's own VCDIFF decoder (each 8 MiB window checked against xdelta3's Adler-32), checks the result's MD5 (`caa546309e0443f7b47632b040a250d7`) and keeps it as `GMSE04/disc.iso` beside `sms.exe`; the player's disc is never modified.
The game is started in its folder with:

```text
SMS_DISC_IMAGE=GMSE04/disc.iso
```

and no mod list: file mods are not applied over Eclipse.

## Mods made for Dolphin

Many GameBanana skins replace `data/mario.szs` with a model built from fewer nodes than Mario's, while the game reaches into Mario's model by index: shape 4, 5, 6 and 10, the cap's joint 1, the `_mat_eye_L`/`_mat_eye_R` materials, a shadow joint's child.
On a GameCube those accesses read past the end of a table; Dolphin without its MMU ignores them, which is why such skins work there (Super Waluigi Sunshine's readme says to turn the MMU off or the game crashes).
Here they crashed while Mario loaded.
`decomp-patches/zzzz-port-mod-model-missing-nodes.patch` gives an index past the end of a model's joint, shape, material or matrix table a stand-in node that no model draws, logs each missing index once (`[j3d] a model has no shape 4 (it has 2)`), and always applies BetterSunshineEngine's guard for a shadow joint with no child.
Mario's own model has every node, so the game is unchanged without mods.

Tried with three skins from GameBanana, installed as CubeShelf installs them (a loose `mario.szs`, placed at `/data/mario.szs`): Garfield (248171), Super Waluigi Sunshine (248164) and Mario 64 Swap's raw file (448379).
Each replaced Mario in Bianco Hills, and Garfield and Mario 64 Swap played seven more stages (Delfino Plaza, Ricco, Gelato, Hotel Delfino, Pinna Park, Noki, the Corona boss) without a crash.
Frame times matched the unmodded game measured in the same session.

Cutscene Removal (50018) ships its movies as THP 1.0 files, which the game's THP player refuses to open (it takes 1.1 only).
On a GameCube the movie director's setup thread then ends with that failure as its exit value, left in r3, and the director moves on at once: that is how the mod removes the cutscenes.
The port had made those thread entry points return null, so the director waited forever on a black screen; `decomp-patches/zzzz-port-thread-exit-values.patch` returns the callee's result again.
With the mod, Delfino Plaza's arrival movie ends at once (frame 105 instead of 3247) and the stage follows.

## Checked

A stage tour with three test mods (two replacing `bianco0.szs` at different priorities, one shipping a loose `airport0.szs`), a blank line, a comment, a relative path, a missing mod and a BOM: Bianco Hills loaded the higher priority mod's archive, the loose file reached `/data/scene/airport0.szs`, and the missing mod was skipped.
The same run from an extracted package, started like CubeShelf starts it (working directory beside `sms.exe`, MSYS2 off the `PATH`), behaved the same.
