#include <Enemy/BossHanachan.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
// TODO: the string literals below compile to one merged anonymous rodata
// blob (all entries referencing "...rodata.0"+offset), but the target
// splits each string into its own local object (@1940, @1941, ...).
// The relocation offsets and pointed-to bytes are identical to the target,
// so this is byte-for-byte correct once linked, but the unlinked .o differs
// from target's per-literal symbol layout. Root cause not yet understood.
static const char* bosshanachan_bastable[] = {
	"/scene/bosshanachan/bas/hanabody_damage.bas",
	nullptr,
	nullptr,
	"/scene/bosshanachan/bas/hanabody_getup_l2.bas",
	"/scene/bosshanachan/bas/hanabody_getup_l3.bas",
	nullptr,
	"/scene/bosshanachan/bas/hanabody_getup_r2.bas",
	"/scene/bosshanachan/bas/hanabody_getup_r3.bas",
	"/scene/bosshanachan/bas/hanabody_hipdrop_long.bas",
	nullptr,
	nullptr,
	"/scene/bosshanachan/bas/hanabody_jitabata2.bas",
	"/scene/bosshanachan/bas/hanabody_jump_reaction.bas",
	nullptr,
	"/scene/bosshanachan/bas/hanabody_pikupiku2.bas",
	"/scene/bosshanachan/bas/hanabody_run.bas",
	"/scene/bosshanachan/bas/hanabody_start.bas",
	nullptr,
	nullptr,
	"/scene/bosshanachan/bas/hanabody_walk.bas",
	nullptr,
	"/scene/bosshanachan/bas/hanahead_end.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/bosshanachan/bas/hanahead_hipdrop_reaction.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/bosshanachan/bas/hanahead_start.bas",
	"/scene/bosshanachan/bas/hanahead_tumble_L.bas",
	"/scene/bosshanachan/bas/hanahead_tumble_R.bas",
	nullptr,
	nullptr,
};

const char** TBossHanachanPartsBase::getBasNameTable() const
{
	return bosshanachan_bastable;
}
