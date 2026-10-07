# GameBanana native integration

The asynchronous service uses GameBanana game 5798 (Super Mario Sunshine):

- `/apiv11/Mod/Index?_aFilters[Generic_Game]=5798&_nPerpage=20&_nPage=N`
- `/apiv11/Mod/ID?_csvProperties=_idRow,_sName,_aGame,_aFiles`

These API routes were checked against GameBanana on 7 October 2026. Each
download rechecks that the mod belongs to game 5798 and that the selected file
is still advertised by that mod. The API's MD5 is checked when supplied; this
detects corrupted downloads, and is not a signature or compatibility proof.

## Menu integration

Include `gamebanana.h`. Catalogue, file-list and install methods return
immediately; `gamebanana_status()` returns a mutex-protected snapshot. Ignore
additional actions while Loading or Downloading. Escape catalogue names and
messages when inserting them into RmlUi markup.

After Installed, rescan installed mods. For file mods, `installed_mod` is the
value accepted by `SMS_MOD` / the `mod` setting. For textures, the directory is
already below the native high-resolution texture loader's `mods/textures`.
Runtime activation/reloading is the menu/runtime's responsibility. Installing
an archive does not mean its regional assets match PAL; author requirements
remain relevant. No console patch is applied to the native executable.

## Supported payloads

ZIP archives containing file replacements below `files/data`, `files/scene`,
`files/sound`, `files/movie`, `files/card`, or equivalent archive-root folders,
are installed to `mods/gamebanana-MOD-FILE/files`. Files must have recognized
game-asset extensions. A ZIP containing only Dolphin-style `tex1_*.png` or
`tex1_*.dds` payloads is installed under `mods/textures/gamebanana-MOD-FILE`.

Console binaries, disc images, patch formats, scripts and executable code are
rejected. RAR/7z archives and ambiguous layouts are reported Unsupported;
external links or README-only releases are not presented as installed mods.
Archives are never executed. Existing destinations, original disc data,
settings and saves are never overwritten.

Archives have a 512 MiB download limit, 2 GiB expanded limit and 20,000-entry
limit. Traversal, absolute/alternate-stream paths, Windows reserved filenames,
ZIP symbolic links, duplicate case-insensitive paths and redirected install
parents are rejected. Installation stages into a separate directory and moves
the completed directory into place only when all payload files were accepted.
Downloaded archives and helper/result diagnostics remain in
`mods/.gamebanana-cache` for troubleshooting. Windows PowerShell and .NET are
required; no Python or external archive executable is used.
