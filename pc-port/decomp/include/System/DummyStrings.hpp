#ifndef SYSTEM_DUMMY_STRINGS_HPP
#define SYSTEM_DUMMY_STRINGS_HPP

// The message comes before the zero object of System/DummyMactorString.hpp,
// which this header includes after it. 220 retail TUs number that zero object
// @1490, so an early common include emits it long before this header comes
// in (the message ids vary from @1525 to @2421); only BathWaterManager includes
// this header first, and there the message is @1900 and the zero object @1907.
// So a TU that carries the zero object ahead of the message (every other one)
// must include System/DummyMactorString.hpp earlier itself.
//
// TODO: still no idea what header this actually was.
//
// TODO: the map lists this 4-byte pointer in .sdata2, always UNUSED, in all
// 292 retail TUs that carry it, so retail spelled it `const char* const` (a
// `char* const` would look the same). That spelling links to the same DOL,
// but the extra leading .sdata2 object shifts objdiff's pairing of the
// anonymous .sdata2 objects against the split retail objects, whose copy was
// dead-stripped: 18 matched units drop in matched_data (Total 99.84% ->
// 88.54%). Kept non-const until objdiff scores that section without it.
static const char* SMS_NO_MEMORY_MESSAGE = "メモリが足りません\n";

#include <System/DummyMactorString.hpp>

// The `cDirtyFileName`/`cDirtyTexName` pollution-texture pair is a third
// member of this family, `(object,local)` in twenty retail TUs (MarNameRefGen,
// MarNameRefGen_BossEnemy, MarDirectorPreEntry, emario, enemyMario, cameragc,
// MapObjFence and thirteen Player TUs, four of them dead-stripped). Its header
// is also unidentified, and intersecting the include closures of all twenty
// yields no game header at all, so it is a rogue-include carrier like these
// two rather than a natural dependency. What the .rodata blobs do pin down:
// the pair always follows this pair, and its position relative to the four
// M3DUtil/InfectiousStrings.hpp mtx-calc names varies per TU (after them in
// cameragc, MapObjFence and MarNameRefGen; before them in
// MarNameRefGen_BossEnemy; MarioCap has no mtx-calc names at all), and
// MapObjBall carries the mtx-calc names without the pair. So the pair's header
// includes this one and is independent of InfectiousStrings.hpp.

#endif
