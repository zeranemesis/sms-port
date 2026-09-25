#include <Enemy/AmiNoko.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/Spine.hpp>

static const char* amiNoko_bastable[] = {
	0,
	"/scene/amiNoko/bas/aminoko_flying1_start.bas",
	"/scene/amiNoko/bas/aminoko_hit1.bas",
	0,
	"/scene/amiNoko/bas/aminoko_run1_loop.bas",
	0,
	0,
	"/scene/amiNoko/bas/aminoko_run2_loop.bas",
	0,
	0,
	"/scene/amiNoko/bas/aminoko_turn1_loop.bas",
	0,
	0,
	"/scene/amiNoko/bas/aminoko_turn2_loop.bas",
	0,
	0,
};

const char** TAmiNoko::getBasNameTable() const { return amiNoko_bastable; }

f32 TAmiNoko::getGravityY() const
{
	if (mSpine->getLatestNerve() == &TNerveAmiNokoDie::theNerve())
		return 0.0f;

	return mGravity;
}

// TODO: weak in the original (inline in the class body); defined out of line
// until the vtable/dtor of TAmiNoko are emitted by this TU.
bool TAmiNoko::isCollidMove(THitActor*) { return false; }

TSmallEnemy* TAmiNokoManager::createEnemyInstance() { return 0; }

void TAmiNokoManager::createModelData()
{
	// TODO: 0x10220000 flags not fully decoded (J3DMLF_* bitfield)
	static TModelDataLoadEntry entry[] = {
		{ "aminoko_model1.bmd", 0x10220000, 0 },
		{ 0, 0, 0 },
	};
	createModelDataArray(entry);
}
