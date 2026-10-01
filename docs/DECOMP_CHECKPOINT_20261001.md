# GMSP01 checkpoint — 2026-10-01

This commit saves the accumulated decompilation sources, matching tools and PC
port changes from `C:\sms-aurora-progress` on `claude/aurora-port`.

## Latest available progress report

Source: local `build/GMSP01/report.json`, written at 08:28:44 +02:00 on
2026-10-01. These are the existing report's results; no new build was run for
this publication.

- Exact code match, including libraries: **52.16%**.
- Exact game code match, excluding libraries: **41.54%**.
- Exactly matching functions, including libraries: **10,433**.
- Target: European GameCube release **GMSP01**.

Earlier object comparisons established an exact match for the 20,080-byte
`TMario::TMario()` constructor. Other large functions remain partial, including
Mario message handling, GCConsole2, camera movement, ModelGate portals and boss
logic. A high similarity score does not mean a function matches exactly.

These percentages measure GameCube code matching. They do not measure the
functional completeness of the PC port.

## Aurora changes

The pinned Aurora revision remains
`514339438178ef2bed1b14e5149d90ece0c6e0cc`.
`patches/checkpoints/aurora-20261001.patch` records all tracked local Aurora
changes relative to that revision, including renderer, FIFO, card and DVD
changes. From a clean checkout of that revision, apply this consolidated patch:

```powershell
git -C extern/aurora apply ../../patches/checkpoints/aurora-20261001.patch
```

The consolidated patch replaces application of the individual `aurora-*.patch`
files for this checkpoint. Do not apply both sets to the same checkout.
It is kept in a subdirectory so the existing patch tool's glob does not apply
the consolidated patch along with the individual patches.

## Local files

Temporary experiments, object files, build outputs, disc images and extracted
game assets are not part of this checkpoint. They remain in the local workspace.
