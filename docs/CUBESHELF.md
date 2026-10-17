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

Disc patches (`.xdelta`, `.vcdiff`, `.bps`, `.ips`) build a different disc and are not applied; the log names them.
Super Mario Eclipse is one: it also changes the game's code (see [ECLIPSE.md](ECLIPSE.md)).
CubeShelf reports such a mod as unusable on its mod page.

## Checked

A stage tour with three test mods (two replacing `bianco0.szs` at different priorities, one shipping a loose `airport0.szs`), a blank line, a comment, a relative path, a missing mod and a BOM: Bianco Hills loaded the higher priority mod's archive, the loose file reached `/data/scene/airport0.szs`, and the missing mod was skipped.
The same run from an extracted package, started like CubeShelf starts it (working directory beside `sms.exe`, MSYS2 off the `PATH`), behaved the same.
