#include <Enemy/SleepBossHanachan.hpp>


// rogue include: the original TU opens .rodata with the dummy string
// pair from System/DummyStrings.hpp; without it every string offset in
// this object is shifted.
#include <System/DummyStrings.hpp>
#include <Enemy/BossHanachan.hpp>

static const char* sleepBossHanachan_bastable[] = {
	"/scene/sleepBossHanachan/bas/demohanatyan_fall.bas",
	nullptr,
};

void TSleepBossHanachan::calcRootMatrix() {}

const char** TSleepBossHanachan::getBasNameTable() const
{
	return sleepBossHanachan_bastable;
}

int TNerveSBH_SleepContinue::execute(TSpineBase<TLiveActor>*) const
{
	return false;
}

// TODO: the remaining 12 functions in this unit (TSleepBossHanachan::init,
// ::startFall, ::~TSleepBossHanachan, TSleepBossHanachanManager and its
// ::createModelData/::~TSleepBossHanachanManager, and the two nerve classes
// TNerveSBH_Fall / TNerveSBH_SleepContinue with their theNerve()/execute()/
// dtors) were not attempted in the time budget available for this pass.
