# Airport decompilation scope

The project target is a playable sequence from boot through the airport, not
the opening movie.  The working slices are: boot and menus, stage/scenario
archive selection, Mario and camera update, map and object loading, HUD/save
UI, and sound.

Use `python tools/airport_progress.py` after `python -m ninja changes_all` to
see a reproducible candidate inventory.  The script filters the objdiff report
to the modules that plausibly serve those slices.  It deliberately does not
claim that every listed module is reachable from the airport, nor that omitted
modules are irrelevant; call-graph evidence is still required before promoting
a candidate to the critical path.

The immediate critical path is:

1. `TApplication::mountStageArchive` must load `TScenarioArchiveName::unkC`,
   the archive-path field populated by scenario parsing.
2. Application video timing must use the retail real-VSync helper at boot,
   keeping PAL and NTSC fade rates equivalent to the retail binary.
3. The stage director, Mario, camera, map and sound candidates then need
   function-level matching and runtime validation in the airport.
